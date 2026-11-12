#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <components/container_plan.hpp>
#include <converters/shipments.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <map>
#include <models/container_totals.hpp>
#include <models/pg/pending_container_creation.hpp>
#include <models/pg/shipment_item.hpp>
#include <models/pg/shipment_raw.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
using namespace helpers;
using namespace converters;

ContainerPlanComponent::ContainerPlanComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()),
      settings_(context.FindComponent<SettingsComponent>()) {}

userver::yaml_config::Schema ContainerPlanComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

namespace {
ContainerPlan ReconcileContainerPlan(const ContainerPlanSnapshot& snapshot,
    const clients::transportations::ApiV1TransportationsContainerFact::Response& facts) {
    const auto& items = snapshot.Items;
    const auto& pending = snapshot.Pending;
    ContainerPlan result;
    std::set<ContainerKey> planned;
    for (const auto& item : items)
        planned.emplace(item.ContainerType, item.Ownership);
    for (const auto& group : facts.groups) {
        std::optional<std::string> ownership;
        if (group.container_ownership.has_value())
            ownership =
                clients::transportations::models::EnumValue(group.container_ownership.value());
        if (group.container_type_code.has_value() && ownership.has_value()) {
            const ContainerKey key{group.container_type_code.value(), ownership.value()};
            result.Totals[key].Created += group.quantity;
            if (planned.contains(key))
                continue;
        }
        models::ContainerFact unplanned;
        unplanned.container_type_code = group.container_type_code;
        if (ownership.has_value())
            unplanned.container_ownership =
                models::ParseEnumScalar(ownership.value(), models::ContainerOwnership{});
        unplanned.quantity = group.quantity;
        result.Unplanned.push_back(std::move(unplanned));
    }
    std::map<std::string, std::int64_t> materialized;
    for (const auto& operation : facts.operations)
        materialized.emplace(operation.operation_id, operation.created_quantity);
    for (const auto& operation : pending)
        result.Totals[{operation.ContainerType, operation.Ownership}].Pending +=
            std::max<std::int64_t>(
                0, operation.Quantity - materialized[boost::uuids::to_string(operation.Id)]);
    return result;
}

}  // namespace

models::ShipmentItem ContainerPlanComponent::AddPlannedContainer(
    const models::AddItemRequest& request, const platform::auth::Identity& actor) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(actor);
    const auto replay =
        FindReplay<models::ShipmentItem>(transaction, organization, "add_item", request);
    if (replay.has_value()) {
        transaction.Commit();
        return replay.value();
    }
    const auto shipment =
        transaction.Execute(sql::kLockShipment, organization, request.shipment_id);
    if (shipment.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    const auto previous = shipment.AsSingleRow<models::pg::ShipmentRaw>();
    CheckRevision(previous.Revision, request.expected_revision);
    const auto settings = settings_.Effective(
        transaction, organization, boost::uuids::to_string(previous.CustomerId));
    if (!settings.PlannedContainers)
        throw ApiError<400>{
            "planned_containers_disabled", "Плановые контейнеры отключены для организации"};
    if (std::find(settings.ContainerTypes.begin(), settings.ContainerTypes.end(),
            request.container_type_code) == settings.ContainerTypes.end())
        throw ApiError<400>{"invalid_container_type", "Тип контейнера недоступен для организации"};
    const auto row =
        transaction
            .Execute(sql::kAddItem, organization, request.shipment_id, request.container_type_code,
                models::EnumValue(request.container_ownership), request.planned_quantity)
            .AsSingleRow<models::pg::ShipmentItem>();
    transaction.Execute(sql::kBumpShipmentRevision, organization, request.shipment_id);
    const auto result = Convert(row);
    SaveReplay(transaction, organization, "add_item", request, result);
    WriteAudit(transaction, organization, actor, "add_item", result.id, request, result);
    transaction.Commit();
    return result;
}

ContainerPlanSnapshot ContainerPlanComponent::LoadPlan(
    const std::string& organization, const std::string& shipment_id) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    if (transaction.Execute(sql::kGetShipment, organization, shipment_id).IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    auto rows = transaction.Execute(sql::kListItems, organization, shipment_id)
                    .AsContainer<std::vector<models::pg::ShipmentItem>>();
    auto pending = transaction.Execute(sql::kPendingContainerCreations, organization, shipment_id)
                       .AsContainer<std::vector<models::pg::PendingContainerCreation>>();
    transaction.Commit();
    return {.Items = std::move(rows), .Pending = std::move(pending)};
}

models::ContainerPlanListResult ContainerPlanComponent::BuildPlanPage(
    const ContainerPlanSnapshot& snapshot,
    const clients::transportations::ApiV1TransportationsContainerFact::Response& facts,
    const models::ListItemsRequest& request) const {
    auto plan = ReconcileContainerPlan(snapshot, facts);
    models::ContainerPlanListResult result;
    result.unplanned_items = std::move(plan.Unplanned);
    const auto after = request.after.has_value()
        ? std::optional{boost::uuids::string_generator{}(request.after.value())}
        : std::nullopt;
    for (const auto& row : snapshot.Items) {
        if (after.has_value() && row.Id <= after.value())
            continue;
        if (result.items.size() >= static_cast<std::size_t>(request.limit))
            break;
        result.items.push_back(Convert(row, plan.Totals[{row.ContainerType, row.Ownership}]));
    }
    return result;
}

models::ShipmentItem ContainerPlanComponent::UpdatePlannedContainer(
    const models::UpdateItemRequest& request, const platform::auth::Identity& actor) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(actor);
    const auto rows = transaction.Execute(sql::kLockItem, organization, request.item_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Позиция не найдена"};
    CheckRevision(rows.AsSingleRow<models::pg::ShipmentItem>().Revision, request.expected_revision);
    const auto row =
        transaction
            .Execute(sql::kUpdateItem, organization, request.item_id, request.planned_quantity)
            .AsSingleRow<models::pg::ShipmentItem>();
    const auto result = Convert(row);
    WriteAudit(transaction, organization, actor, "update_item", result.id, request, result);
    transaction.Commit();
    return result;
}
}  // namespace shipments

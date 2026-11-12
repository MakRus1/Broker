#include <algorithm>
#include <components/container_plan.hpp>
#include <converters/shipments.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <models/pg/pending_container_creation.hpp>
#include <models/pg/shipment_item.hpp>
#include <models/pg/shipment_raw.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
namespace pg = userver::storages::postgres;

ContainerPlanComponent::ContainerPlanComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()),
      settings_(context.FindComponent<SettingsComponent>()) {}

userver::yaml_config::Schema ContainerPlanComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ShipmentItem ContainerPlanComponent::AddPlannedContainer(
    const models::AddItemRequest& request, const platform::auth::Identity& actor) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(actor);
    const auto replay =
        helpers::FindReplay<models::ShipmentItem>(transaction, organization, "add_item", request);
    if (replay.has_value()) {
        transaction.Commit();
        return replay.value();
    }

    const auto shipment =
        transaction.Execute(sql::kLockShipment, organization, request.shipment_id);
    if (shipment.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};

    const auto previous = shipment.AsSingleRow<models::pg::ShipmentRaw>();
    helpers::CheckRevision(previous.Revision, request.expected_revision);

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
    const auto result = converters::Convert(row);
    helpers::SaveReplay(transaction, organization, "add_item", request, result);

    helpers::WriteAudit(transaction, organization, actor, "add_item", result.id, request, result);
    transaction.Commit();
    return result;
}

ContainerPlanSnapshot ContainerPlanComponent::LoadPlan(
    const std::string& organization, const std::string& shipment_id) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadSnapshot);
    if (transaction.Execute(sql::kGetShipment, organization, shipment_id).IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    auto rows = transaction.Execute(sql::kListItems, organization, shipment_id)
                    .AsContainer<std::vector<models::pg::ShipmentItem>>();
    auto pending = transaction.Execute(sql::kPendingContainerCreations, organization, shipment_id)
                       .AsContainer<std::vector<models::pg::PendingContainerCreation>>();
    transaction.Commit();
    return {.Items = std::move(rows), .Pending = std::move(pending)};
}

models::ShipmentItem ContainerPlanComponent::UpdatePlannedContainer(
    const models::UpdateItemRequest& request, const platform::auth::Identity& actor) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(actor);
    const auto rows = transaction.Execute(sql::kLockItem, organization, request.item_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Позиция не найдена"};
    helpers::CheckRevision(
        rows.AsSingleRow<models::pg::ShipmentItem>().Revision, request.expected_revision);

    const auto row =
        transaction
            .Execute(sql::kUpdateItem, organization, request.item_id, request.planned_quantity)
            .AsSingleRow<models::pg::ShipmentItem>();
    const auto result = converters::Convert(row);

    helpers::WriteAudit(
        transaction, organization, actor, "update_item", result.id, request, result);
    transaction.Commit();
    return result;
}
}  // namespace shipments

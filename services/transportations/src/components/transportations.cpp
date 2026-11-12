#include <boost/uuid/uuid_io.hpp>
#include <components/inherited_route.hpp>
#include <components/transportations.hpp>
#include <converters/container_facts.hpp>
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <models/pg/container_fact.hpp>
#include <models/pg/route_segment.hpp>
#include <models/pg/saved_command.hpp>
#include <models/pg/transportation_event.hpp>
#include <models/pg/transportation_raw.hpp>
#include <platform/auth/identity.hpp>
#include <platform/text.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = transportations::models;
using transportations::ApiError;

void ValidateDetails(models::TransportationType type,
    const std::optional<std::string>& container_type,
    const std::optional<models::ContainerOwnership>& ownership,
    const std::optional<std::string>& container_number) {
    if (type != models::TransportationType::Container &&
        (container_type.has_value() || ownership.has_value() || container_number.has_value()))
        throw ApiError<400>{
            "unexpected_container", "Контейнерные поля доступны только для контейнерной перевозки"};
}

}  // namespace

namespace transportations {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

TransportationsComponent::TransportationsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()) {}

userver::yaml_config::Schema TransportationsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::Transportation TransportationsComponent::FindTransportation(
    const std::string& organization, const std::string& id) const {

    auto transaction =
        cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);

    auto result = helpers::ReadTransportation(transaction, organization, id, false);
    transaction.Commit();
    return result;
}

models::TransportationHistoryResult TransportationsComponent::GetTransportationHistory(
    models::ListHistoryRequest request, const std::string& organization) const {
    // Наличие перевозки и её история проверяются в одном снимке.

    auto transaction =
        cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);
    if (transaction.Execute(sql::kGetTransportation, organization, request.transportation_id)
            .IsEmpty())
        throw ApiError<404>{"not_found", "Перевозка не найдена"};
    const auto rows = transaction.Execute(
        sql::kListHistory, organization, request.transportation_id, request.before, request.limit);
    transaction.Commit();
    return models::TransportationHistoryResult{
        .items =
            converters::Convert(rows.AsContainer<std::vector<models::pg::TransportationEvent>>())};
}

models::Transportation TransportationsComponent::CreateTransportation(
    const std::string& organization, const platform::auth::Identity& identity,
    const models::CreateTransportationRequest& request,
    const models::ShipmentContext& source) const {
    ValidateDetails(request.type, request.container_type_code, request.container_ownership,
        request.container_number);

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockCommand, organization, request.id);
    const auto saved = transaction.Execute(sql::kFindCommand, organization, request.id);
    if (!saved.IsEmpty()) {
        const auto command = saved.AsSingleRow<models::pg::SavedCommand>();
        if (command.Payload != helpers::Document(request))
            throw ApiError<409>{"idempotency_conflict", "Содержимое команды создания изменено"};
        // Снимки старых команд содержат технический номер, которого больше нет в API.
        userver::formats::json::ValueBuilder document{command.Result};
        document.Remove("number");
        auto result = models::ParseModelJson(
            document.ExtractValue(), std::type_identity<models::Transportation>{});
        transaction.Commit();
        route_rules::InheritRoute(result, source);
        return result;
    }

    auto result =
        converters::Convert(transaction
                                .Execute(sql::kCreateManualTransportation, organization, request.id,
                                    request.shipment_id, models::EnumValue(request.type),
                                    request.hazardous, request.container_type_code,
                                    converters::ConvertOwnership(request.container_ownership),
                                    platform::TrimToOptional(request.container_number))
                                .AsSingleRow<models::pg::TransportationRaw>());
    route_rules::InheritRoute(result, source);
    transaction.Execute(sql::kInsertCommand, organization, request.id, helpers::Document(request),
        helpers::Document(result));

    helpers::WriteAudit(
        transaction, organization, identity, "create_transportation", result.id, request, result);
    transaction.Commit();
    return result;
}

models::Transportation TransportationsComponent::UpdateTransportation(
    const std::string& organization, const platform::auth::Identity& identity,
    const models::UpdateTransportationRequest& request,
    const models::ShipmentContext& source) const {
    ValidateDetails(request.type, request.container_type_code, request.container_ownership,
        request.container_number);

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    auto current =
        helpers::ReadTransportation(transaction, organization, request.transportation_id, true);
    helpers::CheckRevision(current.revision, request.expected_revision);

    auto result = converters::Convert(
        transaction
            .Execute(sql::kUpdateTransportation, organization, request.transportation_id,
                models::EnumValue(request.type), request.hazardous, request.container_type_code,
                converters::ConvertOwnership(request.container_ownership),
                platform::TrimToOptional(request.container_number))
            .AsSingleRow<models::pg::TransportationRaw>());
    result.route_segments = std::move(current.route_segments);
    route_rules::InheritRoute(result, source);

    helpers::WriteAudit(
        transaction, organization, identity, "update_transportation", result.id, request, result);
    transaction.Commit();
    return result;
}

models::ContainerFactsResult TransportationsComponent::GetContainerFacts(
    models::ContainerFactsRequest request, const std::string& organization) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kContainerFacts,
        organization, request.shipment_id, request.pending_operation_ids);
    return converters::Convert(rows.AsContainer<std::vector<models::pg::ContainerFact>>());
}

models::EnsureTransportationsResult TransportationsComponent::CreateTransportationsFromCommand(
    const models::EnsureCreatedRequest& request, const platform::auth::Identity& identity) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = request.organization_id;
    transaction.Execute(sql::kLockCommand, organization, request.operation_id);
    const auto rows = transaction.Execute(sql::kFindCommand, organization, request.operation_id);
    if (!rows.IsEmpty()) {
        const auto saved = rows.AsSingleRow<models::pg::SavedCommand>();
        if (saved.Payload != helpers::Document(request))
            throw ApiError<409>{"protocol_conflict", "Содержимое операции изменено"};
        const auto result =
            models::ParseModelJson(saved.Result, std::type_identity<models::CreationOperation>{});
        transaction.Commit();
        return models::EnsureTransportationsResult{.operation = result};
    }

    models::CreationOperation result;
    result.id = request.operation_id;
    result.item_id = request.item_id;
    result.quantity = request.quantity;
    result.status = models::OperationStatus::Pending;
    transaction.Execute(sql::kInsertCommand, organization, request.operation_id,
        helpers::Document(request), helpers::Document(result));
    for (std::int64_t ordinal = 1; ordinal <= request.quantity; ++ordinal) {
        result.transportation_ids.push_back(
            transaction
                .Execute(sql::kCreateTransportation, organization, request.shipment_id,
                    request.item_id, request.operation_id, ordinal, request.container_type_code,
                    models::EnumValue(request.container_ownership))
                .AsSingleRow<std::string>());
    }
    result.status = models::OperationStatus::Succeeded;
    transaction.Execute(
        sql::kCompleteCommand, organization, request.operation_id, helpers::Document(result));

    helpers::WriteAudit(
        transaction, organization, identity, "ensure_created", result.id, request, result);
    transaction.Commit();
    return models::EnsureTransportationsResult{.operation = result};
}
}  // namespace transportations

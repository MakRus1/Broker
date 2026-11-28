#include <boost/uuid/uuid_io.hpp>
#include <components/inherited_route.hpp>
#include <components/routes.hpp>
#include <components/transportations.hpp>
#include <configs/keys.hpp>
#include <converters/container_facts.hpp>
#include <converters/route.hpp>
#include <converters/saved_transportation.hpp>
#include <converters/transportation_history.hpp>
#include <converters/transportations.hpp>
#include <exception>
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_usage.hpp>
#include <map>
#include <models/pg/container_fact.hpp>
#include <models/pg/saved_command.hpp>
#include <models/pg/transportation_event.hpp>
#include <models/pg/transportation_raw.hpp>
#include <platform/auth/identity.hpp>
#include <platform/db.hpp>
#include <platform/text.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text.hpp>

namespace {
namespace models = transportations::models;
using transportations::ApiError;

std::optional<boost::uuids::uuid> CurrentSegmentId(const userver::formats::json::Value& route) {
    // Последний участок с фактом определяет положение перевозки.
    for (auto index = route.GetSize(); index > 0; --index) {
        const auto segment = transportations::converters::Convert(route[index - 1]);
        if (!segment.inherited.value_or(true))
            continue;

        if (segment.actual_arrival_at.has_value())
            return std::nullopt;
        if (segment.actual_departure_at.has_value())
            return segment.id;
    }
    return std::nullopt;
}

void ValidateDetails(models::TransportationType type,
    const std::optional<std::string>& container_type,
    const std::optional<models::ContainerOwnership>& ownership,
    const std::optional<std::string>& container_number, bool has_iso_code) {
    if (type != models::TransportationType::Container &&
        (container_type.has_value() || ownership.has_value() || container_number.has_value() ||
            has_iso_code))
        throw ApiError<400>{
            "unexpected_container", "Контейнерные поля доступны только для контейнерной перевозки"};
}

void ValidateContainerCatalog(const platform::configs::Json& catalog,
    const std::optional<std::string>& group_code, const std::optional<std::string>& iso_code,
    bool group_changed) {
    const bool known = group_code.has_value() && catalog.HasMember(group_code.value());
    if (group_changed && group_code.has_value() &&
        (!known || !catalog[group_code.value()]["active"].As<bool>()))
        throw ApiError<400>{"invalid_container_type", "Выберите активный тип контейнера"};
    if (!iso_code.has_value())
        return;
    if (known) {
        for (const auto& allowed : catalog[group_code.value()]["iso_codes"])
            if (allowed.As<std::string>() == iso_code.value())
                return;
    }
    throw ApiError<400>{
        "invalid_container_iso_code", "Код ISO не соответствует выбранному типу контейнера"};
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
                   .GetCluster()),
      configs_(context.FindComponent<userver::components::DynamicConfig>().GetSource()),
      routes_(context.FindComponent<RoutesComponent>()) {}

models::Transportation TransportationsComponent::FindTransportation(
    const boost::uuids::uuid& organization, const boost::uuids::uuid& id) const {

    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetTransportation, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Перевозка не найдена"};
    return converters::Convert(rows.AsSingleRow<models::pg::TransportationRaw>());
}

models::TransportationHistoryResult TransportationsComponent::GetTransportationHistory(
    const boost::uuids::uuid& organization, const models::ListHistoryRequest& request) const {
    // Наличие перевозки и её история проверяются в одном снимке.

    auto transaction =
        cluster_->Begin(pg::ClusterHostType::kMaster, platform::db::kReadOnlySnapshot);
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
    const boost::uuids::uuid& organization, const platform::auth::Identity& identity,
    const models::CreateTransportationRequest& request, const models::ShipmentContext& source,
    std::string_view access_token) const {
    ValidateDetails(request.type, request.container_type_code, request.container_ownership,
        request.container_number, request.container_iso_code.has_value());

    const auto replay = [&](const pg::ResultSet& saved) {
        const auto command = saved.AsSingleRow<models::pg::SavedCommand>();
        if (command.Payload != helpers::Document(request) &&
            helpers::Document(models::ParseModelJson(
                command.Payload, std::type_identity<models::CreateTransportationRequest>{})) !=
                helpers::Document(request))
            throw ApiError<409>{"idempotency_conflict", "Содержимое команды создания изменено"};
        auto result = converters::RestoreTransportation(command.Result);
        route_rules::InheritRoute(result, source);
        return result;
    };
    const auto previous = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kFindCommand, organization, request.id);
    if (!previous.IsEmpty())
        return replay(previous);

    models::Transportation prepared{};
    prepared.id = request.id;
    std::optional<std::int64_t> usage_check_version;
    std::exception_ptr preparation_error;
    try {
        if (request.container_type_code.has_value() || request.container_iso_code.has_value())
            ValidateContainerCatalog(configs_.GetSnapshot().Get(configs::CONTAINER_CATALOG),
                request.container_type_code, request.container_iso_code, true);

        if (request.route_segments.has_value()) {
            models::SetRouteRequest route;
            route.transportation_id = request.id;
            route.route_segments = request.route_segments.value();
            usage_check_version =
                routes_.PrepareRouteWrite(organization, access_token, route, source, prepared);
        } else {
            route_rules::InheritRoute(prepared, source);
        }
    } catch (const platform::http::HandlerErrorBase&) {
        // Пока шла подготовка, другой запрос мог сохранить команду с тем же ключом.
        preparation_error = std::current_exception();
    }

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockCommand, organization, request.id);
    const auto saved = transaction.Execute(sql::kFindCommand, organization, request.id);
    if (!saved.IsEmpty()) {
        auto result = replay(saved);
        transaction.Commit();
        return result;
    }
    if (preparation_error)
        std::rethrow_exception(preparation_error);
    helpers::CheckRouteWrite(transaction, organization, source.shipment_id, usage_check_version);
    auto result = converters::Convert(
        transaction
            .Execute(sql::kCreateManualTransportation, organization, request.id,
                request.shipment_id, models::EnumValue(request.type), request.hazardous,
                request.container_type_code,
                converters::ConvertOwnership(request.container_ownership),
                platform::TrimToOptional(request.container_number), request.container_iso_code)
            .AsSingleRow<models::pg::TransportationRaw>());
    result.route_segments = std::move(prepared.route_segments);
    result.incoterms = source.incoterms;
    result.loading = source.loading;
    result.carriage_terms = source.carriage_terms;
    if (request.route_segments.has_value())
        transaction.Execute(sql::kSaveRouteSegments, organization, result.id,
            converters::ToStoredRoute(result.route_segments));
    transaction.Execute(sql::kInsertCommand, organization, request.id, helpers::Document(request),
        helpers::Document(result));

    helpers::WriteAudit(
        transaction, organization, identity, "create_transportation", result.id, request, result);
    transaction.Commit();
    return result;
}

models::Transportation TransportationsComponent::UpdateTransportation(
    const boost::uuids::uuid& organization, const platform::auth::Identity& identity,
    models::UpdateTransportationRequest& request, const models::ShipmentContext& source,
    std::string_view access_token, models::Transportation current) const {
    const auto requested_iso = request.container_iso_code.value_or(std::nullopt);
    ValidateDetails(request.type, request.container_type_code, request.container_ownership,
        request.container_number, requested_iso.has_value());

    const bool group_changed = current.container_type_code != request.container_type_code;
    const auto iso_code = request.container_iso_code.has_value() || group_changed ||
            request.type != models::TransportationType::Container
        ? requested_iso
        : current.container_iso_code;
    if (group_changed || iso_code != current.container_iso_code)
        ValidateContainerCatalog(configs_.GetSnapshot().Get(configs::CONTAINER_CATALOG),
            request.container_type_code, iso_code, group_changed);

    std::optional<std::int64_t> usage_check_version;
    if (request.route_segments.has_value()) {
        models::SetRouteRequest route{.expected_revision = request.expected_revision,
            .route_segments = std::move(request.route_segments.value()),
            .transportation_id = request.transportation_id};
        usage_check_version =
            routes_.PrepareRouteWrite(organization, access_token, route, source, current);
        request.route_segments = std::move(route.route_segments);
    } else {
        route_rules::InheritRoute(current, source);
    }
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    helpers::CheckRouteWrite(transaction, organization, source.shipment_id, usage_check_version);
    const auto updated = transaction.Execute(sql::kUpdateTransportation, organization,
        request.transportation_id, models::EnumValue(request.type), request.hazardous,
        request.container_type_code, converters::ConvertOwnership(request.container_ownership),
        platform::TrimToOptional(request.container_number), iso_code, request.expected_revision);
    if (updated.IsEmpty())
        throw RevisionConflict{};
    auto result = converters::Convert(updated.AsSingleRow<models::pg::TransportationRaw>());
    result.route_segments = std::move(current.route_segments);
    result.incoterms = source.incoterms;
    result.loading = source.loading;
    result.carriage_terms = source.carriage_terms;
    if (request.route_segments.has_value())
        transaction.Execute(sql::kSaveRouteSegments, organization, result.id,
            converters::ToStoredRoute(result.route_segments));

    helpers::WriteAudit(
        transaction, organization, identity, "update_transportation", result.id, request, result);
    transaction.Commit();
    return result;
}

models::ShipmentRouteProgress TransportationsComponent::GetShipmentRouteProgress(
    const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment_id) const {
    const auto routes = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kShipmentRouteProgress, organization, shipment_id);
    models::ShipmentRouteProgress result{};
    std::map<boost::uuids::uuid, std::int64_t> counts;
    for (const auto& route : routes.AsSetOf<userver::formats::json::Value>()) {
        ++result.total;
        const auto segment_id = CurrentSegmentId(route);
        if (segment_id.has_value())
            ++counts[segment_id.value()];
    }
    for (const auto& [segment_id, quantity] : counts)
        result.current.push_back({.quantity = quantity, .route_segment_id = segment_id});
    return result;
}

models::ContainerFactsResult TransportationsComponent::GetContainerFacts(
    const boost::uuids::uuid& organization, const models::ContainerFactsRequest& request) const {
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
        if (saved.Payload != helpers::Document(request) &&
            helpers::Document(models::ParseModelJson(saved.Payload,
                std::type_identity<models::EnsureCreatedRequest>{})) != helpers::Document(request))
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
                .AsSingleRow<boost::uuids::uuid>());
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

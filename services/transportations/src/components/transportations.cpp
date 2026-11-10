#include <boost/uuid/uuid_io.hpp>
#include <components/transportations.hpp>
#include <converters/container_facts.hpp>
#include <converters/transportations.hpp>
#include <domain/inherited_route.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <postgresql/types.hpp>
#include <registry/query.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transportations {
using namespace helpers;
using namespace converters;
using namespace domain;
using namespace registry_query;

TransportationsComponent::TransportationsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      references_(context.FindComponent<ReferencesComponent>()) {}

userver::yaml_config::Schema TransportationsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

namespace {

void ValidateDetails(models::TransportationType type,
    const std::optional<std::string>& container_type,
    const std::optional<models::ContainerOwnership>& ownership,
    const std::optional<std::string>& container_number) {
    if (type != models::TransportationType::Container &&
        (container_type.has_value() || ownership.has_value() || container_number.has_value()))
        throw ApiError<400>{
            "unexpected_container", "Контейнерные поля доступны только для контейнерной перевозки"};
}

std::optional<std::string> CleanContainerNumber(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    auto result = userver::utils::text::Trim(value.value());
    if (result.empty())
        return std::nullopt;
    return result;
}

std::optional<std::string> Ownership(const std::optional<models::ContainerOwnership>& value) {
    if (!value.has_value())
        return std::nullopt;
    return std::string{models::EnumValue(value.value())};
}
}  // namespace

models::Transportation TransportationsComponent::LoadLocal(
    const userver::server::request::RequestContext& context, const std::string& id) const {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    auto result = ReadTransportation(transaction, organization, id, false);
    transaction.Commit();
    return result;
}

models::Transportation TransportationsComponent::Load(
    const userver::server::request::RequestContext& context, const std::string& id) const {
    auto result = LoadLocal(context, id);
    const auto contexts = references_.LoadShipmentContexts(context, {result.shipment_id});
    InheritRoute(result, FindShipmentContext(contexts, result.shipment_id));
    return result;
}

models::TransportationHistoryResult TransportationsComponent::History(
    models::ListHistoryRequest request, userver::server::request::RequestContext& context) const {

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
    // Наличие перевозки и её история проверяются в одном снимке.
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    const auto organization = platform::auth::GetOrganization(identity);
    if (transaction.Execute(sql::kGetTransportation, organization, request.transportation_id)
            .IsEmpty())
        throw ApiError<404>{"not_found", "Перевозка не найдена"};
    const auto rows = transaction.Execute(
        sql::kListHistory, organization, request.transportation_id, request.before, request.limit);
    transaction.Commit();
    return models::TransportationHistoryResult{
        .items = Convert(rows.AsContainer<std::vector<pgtypes::TransportationEventV2>>())};
}

models::TransportationResult TransportationsComponent::Get(models::GetTransportationRequest request,
    userver::server::request::RequestContext& context) const {

    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
    const auto result = Load(context, request.transportation_id);
    return models::TransportationResult{.transportation = result};
}

models::TransportationListResult TransportationsComponent::List(
    models::ListTransportationsRequest request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto scope = CursorScope(request, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, request.sort);
        if (request.after.has_value() &&
            (request.cursor.has_value() || !order.ById || request.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (request.cursor.has_value() && request.cursor.value().scope != scope)
            throw ApiError<400>{
                "invalid_cursor", "Фильтры или сортировка изменились. Начните с первой страницы"};
        const auto query = platform::registry::BuildPage(kRegistry, request, organization);
        // Перевозки и плечи страницы должны относиться к одному снимку.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        auto page = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool has_more = page.size() > static_cast<std::size_t>(request.limit);
        if (has_more)
            page.pop_back();
        std::optional<models::RegistryCursor> next_cursor;
        if (has_more)
            next_cursor =
                models::RegistryCursor{.id = boost::uuids::to_string(page.back().Transportation.Id),
                    .scope = scope,
                    .value = page.back().SortKey};
        const auto rows = Convert(page);
        std::vector<std::string> ids;
        ids.reserve(rows.size());
        for (const auto& row : rows)
            ids.push_back(boost::uuids::to_string(row.Id));
        // Все плечи страницы читаются одним запросом, без N+1.
        const auto route_segments = ids.empty()
            ? std::vector<pgtypes::RouteSegmentV1>{}
            : transaction.Execute(sql::kListRouteSegments, organization, ids)
                  .AsContainer<std::vector<pgtypes::RouteSegmentV1>>();
        auto result = Convert(rows, route_segments);
        transaction.Commit();
        std::set<std::string> unique_shipments;
        for (const auto& item : result)
            unique_shipments.insert(item.shipment_id);
        const auto sources = references_.LoadShipmentContexts(
            context, {unique_shipments.begin(), unique_shipments.end()});
        for (auto& item : result)
            InheritRoute(item, FindShipmentContext(sources, item.shipment_id));
        return models::TransportationListResult{.items = result, .next_cursor = next_cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::TransportationResult TransportationsComponent::Create(
    models::CreateTransportationRequest request,
    userver::server::request::RequestContext& context) const {

    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    ValidateDetails(request.type, request.container_type_code, request.container_ownership,
        request.container_number);
    const auto sources = references_.LoadShipmentContexts(context, {request.shipment_id});
    const auto& source = FindShipmentContext(sources, request.shipment_id);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockCommand, organization, request.id);
    const auto saved = transaction.Execute(sql::kFindCommand, organization, request.id);
    if (!saved.IsEmpty()) {
        const auto command = saved.AsSingleRow<pgtypes::SavedCommandV1>();
        if (command.Payload != Document(request))
            throw ApiError<409>{"idempotency_conflict", "Содержимое команды создания изменено"};
        // Снимки старых команд содержат технический номер, которого больше нет в API.
        userver::formats::json::ValueBuilder document{command.Result};
        document.Remove("number");
        auto result = models::ParseModelJson(
            document.ExtractValue(), std::type_identity<models::Transportation>{});
        transaction.Commit();
        InheritRoute(result, source);
        return models::TransportationResult{.transportation = result};
    }
    auto result =
        Convert(transaction
                    .Execute(sql::kCreateManualTransportation, organization, request.id,
                        request.shipment_id, models::EnumValue(request.type), request.hazardous,
                        request.container_type_code, Ownership(request.container_ownership),
                        CleanContainerNumber(request.container_number))
                    .AsSingleRow<pgtypes::TransportationV4>());
    InheritRoute(result, source);
    transaction.Execute(
        sql::kInsertCommand, organization, request.id, Document(request), Document(result));
    WriteAudit(
        transaction, organization, identity, "create_transportation", result.id, request, result);
    transaction.Commit();
    return models::TransportationResult{.transportation = result};
}

models::TransportationResult TransportationsComponent::Update(
    models::UpdateTransportationRequest request,
    userver::server::request::RequestContext& context) const {

    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    ValidateDetails(request.type, request.container_type_code, request.container_ownership,
        request.container_number);
    auto loaded = Load(context, request.transportation_id);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto current = ReadTransportation(transaction, organization, loaded.id, true);
    CheckRevision(current.revision, request.expected_revision);
    auto result =
        Convert(transaction
                    .Execute(sql::kUpdateTransportation, organization, loaded.id,
                        models::EnumValue(request.type), request.hazardous,
                        request.container_type_code, Ownership(request.container_ownership),
                        CleanContainerNumber(request.container_number))
                    .AsSingleRow<pgtypes::TransportationV4>());
    result.route_segments = std::move(loaded.route_segments);
    result.incoterms = loaded.incoterms;
    result.loading = loaded.loading;
    result.carriage_terms = loaded.carriage_terms;
    WriteAudit(
        transaction, organization, identity, "update_transportation", result.id, request, result);
    transaction.Commit();
    return models::TransportationResult{.transportation = result};
}

models::ContainerFactsResult TransportationsComponent::ContainerFacts(
    models::ContainerFactsRequest request,
    userver::server::request::RequestContext& context) const {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kContainerFacts,
        organization, request.shipment_id, request.pending_operation_ids);
    return Convert(rows.AsContainer<std::vector<pgtypes::ContainerFactV1>>());
}

models::RegistryValuesResult TransportationsComponent::FilterValues(
    models::FilterValuesRequest request, userver::server::request::RequestContext& context) const {
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        const bool route = request.scope == models::FilterValuesScope::Route;
        models::RegistryValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(route
                ? std::span<const registry::RegistryField>{route_registry::kFields}
                : std::span<const registry::RegistryField>{registry_fields::kFields});
        if (!request.field.has_value())
            return result;
        if (route && request.candidates.has_value())
            throw registry::FilterError{"Для участка используйте поиск значений, а не candidates"};
        if (route &&
            request.group > static_cast<int>(request.registry.route_filters.has_value()
                                    ? request.registry.route_filters.value().size()
                                    : 0))
            throw registry::FilterError{"Некорректный номер группы участка"};
        if (!route && request.group != 0)
            throw registry::FilterError{"Номер группы допустим только для участка"};
        const auto query = route
            ? route_registry::BuildValues(request.registry, organization, request.field.value(),
                  request.group, request.query, request.after, request.limit,
                  [&](auto& builder) {
                      registry::ApplyFilters(
                          builder, registry_fields::kFields, request.registry.filters);
                  })
            : platform::registry::BuildFilterValues(kRegistry, request, organization);
        // Custom plan сохраняет селективность условий и вариантов связанной коллекции.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        const auto rows = transaction.Execute(query.Statement, query.Parameters)
                              .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        transaction.Commit();
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::EnsureTransportationsResult TransportationsComponent::EnsureCreated(
    models::EnsureCreatedRequest request, userver::server::request::RequestContext& context) const {

    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_CREATE);
    const auto& identity = platform::auth::GetIdentity(context);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = request.organization_id;
    transaction.Execute(sql::kLockCommand, organization, request.operation_id);
    const auto rows = transaction.Execute(sql::kFindCommand, organization, request.operation_id);
    if (!rows.IsEmpty()) {
        const auto saved = rows.AsSingleRow<pgtypes::SavedCommandV1>();
        if (saved.Payload != Document(request))
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
    transaction.Execute(sql::kInsertCommand, organization, request.operation_id, Document(request),
        Document(result));
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
        sql::kCompleteCommand, organization, request.operation_id, Document(result));
    WriteAudit(transaction, organization, identity, "ensure_created", result.id, request, result);
    transaction.Commit();
    return models::EnsureTransportationsResult{.operation = result};
}
}  // namespace transportations

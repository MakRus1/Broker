#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_filters.hpp>

namespace transportations::helpers {
namespace registry = platform::registry;

namespace registry_fields {

// SQL-поля: выражения фильтрации и, при необходимости, отдельный порядок сортировки.
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};

inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};

inline constexpr auto kNumber =
    registry::Field<std::string>{
        "container_number", "lower(COALESCE(container_number, '')) COLLATE \"C\""}
        .SortedAs("lower(COALESCE(container_number, '')) COLLATE public.registry_number_order");

inline constexpr registry::Field<std::string> kType{"type", "type COLLATE \"C\""};

inline constexpr registry::Field<std::string> kContainerType{
    "container_type", "COALESCE(container_type_code, '') COLLATE \"C\""};

inline constexpr registry::Field<std::string> kOwnership{
    "container_ownership", "COALESCE(container_ownership, '') COLLATE \"C\""};

inline constexpr registry::Field<bool> kIncompleteContainer{"incomplete_container",
    "(type = 'container' AND (container_type_code IS NULL OR container_ownership IS NULL))",
    "BOOLEAN"};

inline constexpr auto kHazardous =
    registry::Field<bool>{"hazardous", "hazardous", "BOOLEAN"}.SortedAs(
        "(CASE WHEN hazardous THEN 'true' ELSE 'false' END) COLLATE \"C\"");

inline constexpr registry::Field<std::string> kShipment{"shipment_id", "shipment_id", "UUID"};

inline constexpr registry::Field<std::string> kItem{"item_id", "item_id", "UUID"};

// Каталог API: подписи, доступные операции и источники отображаемых значений.
inline constexpr std::array<registry::RegistryField, 9> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kNumber,
        .Label = "Номер контейнера",
        .ValueSql = "container_number",
        .FoldValues = true},
    {.Field = kType, .Label = "Тип перевозки", .ValueSql = "type"},
    {.Field = kContainerType,
        .Label = "Тип контейнера",
        .ValueSql = "COALESCE(container_type_code, '')"},
    {.Field = kOwnership, .Label = "Принадлежность", .ValueSql = "container_ownership"},
    {.Field = kHazardous, .Label = "Опасный груз", .ValueSql = "hazardous"},
    {.Field = kShipment,
        .Label = "Поставка",
        .ValueSql = "shipment_id",
        .Sortable = false,
        .Lookup = "shipments"},
    {.Field = kItem,
        .Label = "Плановая позиция",
        .ValueSql = "item_id",
        .Sortable = false,
        .Visible = false},
    {.Field = kIncompleteContainer,
        .Label = "Неполные данные контейнера",
        .ValueSql = kIncompleteContainer.Expression,
        .Sortable = false,
        .Visible = false},
}};

inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(
    const models::ListTransportationsRequest& request, const std::string& organization,
    const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListTransportations,
        registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

// Страница реестра: общие условия + курсор + лимит с одной дополнительной записью.
inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListTransportationsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    auto query = BuildRegistryFilters(request, organization);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}

// Варианты одного фильтра: остальные условия сохраняются, собственное исключается.
inline registry::BuiltQuery BuildRegistryValues(const models::ListTransportationsRequest& request,
    const std::string& organization, const std::string& field,
    const std::optional<std::string>& search, const std::optional<std::string>& after, int limit,
    const std::optional<std::vector<std::string>>& candidates = std::nullopt) {
    const auto& definition = registry::FindField(registry_fields::kFields, field);
    auto builder = BuildRegistryFilters(request, organization, field);
    registry::ApplyCandidates(builder, definition, candidates);
    return registry::BuildValues(
        std::move(builder), sql::kRegistryFilterValues, definition, search, after, limit + 1);
}
}  // namespace transportations::helpers

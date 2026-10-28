#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_filters.hpp>

namespace transportations::helpers {
namespace registry = platform::registry;

namespace registry_fields {
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
inline constexpr std::array<registry::RegistryField, 7> kFields{{
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
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

inline registry::QueryBuilder BuildRegistryFilters(
    const models::ListTransportationsRequest& request, const std::string& organization,
    const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListTransportations,
        registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    if (registry::UseLegacyFilter(request.filters, "container_number", exclude))
        query.WhereContains(kNumber, request.query);
    if (registry::UseLegacyFilter(request.filters, "type", exclude))
        query.WhereAny(kType, Convert(request.types));
    if (registry::UseLegacyFilter(request.filters, "container_type", exclude))
        query.WhereAny(kContainerType, request.container_types);
    if (registry::UseLegacyFilter(request.filters, "hazardous", exclude))
        query.WhereEqual(kHazardous, request.hazardous);
    query.WhereEqual(kIncompleteContainer, request.incomplete_container);
    if (registry::UseLegacyFilter(request.filters, "shipment_id", exclude))
        query.WhereEqual(kShipment, request.shipment_id);
    query.WhereEqual(kItem, request.item_id);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListTransportationsRequest& request, const std::string& organization) {
    using namespace registry_fields;
    auto query = BuildRegistryFilters(request, organization);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}

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

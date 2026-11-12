#pragma once
#include <openapi/models.hpp>
#include <platform/registry_page.hpp>
#include <registry/route.hpp>
#include <transportations/sql_queries.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace transportations::registry_query {
using platform::registry::CursorScope;
using platform::registry::SearchPattern;
namespace registry = platform::registry;

namespace registry_fields {

// SQL-поля: выражения фильтрации и, при необходимости, отдельный порядок сортировки.
inline constexpr registry::Field<std::string> kId{"id", "transportations.id", "UUID", true};

inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "transportations.organization_id", "UUID"};

inline constexpr auto kNumber = registry::Field<std::string>{"container_number",
    "lower(COALESCE(transportations.container_number, '')) COLLATE \"C\""}
                                    .SortedAs("lower(COALESCE(transportations.container_number, "
                                              "'')) COLLATE public.registry_number_order");

inline constexpr registry::Field<std::string> kType{"type", "transportations.type COLLATE \"C\""};

inline constexpr registry::Field<std::string> kContainerType{
    "container_type", "COALESCE(transportations.container_type_code, '') COLLATE \"C\""};

inline constexpr registry::Field<std::string> kOwnership{
    "container_ownership", "COALESCE(transportations.container_ownership, '') COLLATE \"C\""};

inline constexpr registry::Field<bool> kIncompleteContainer{"incomplete_container",
    "(transportations.type = 'container' AND (transportations.container_type_code IS NULL OR "
    "transportations.container_ownership IS NULL))",
    "BOOLEAN"};

inline constexpr auto kHazardous =
    registry::Field<bool>{"hazardous", "transportations.hazardous", "BOOLEAN"}.SortedAs(
        "(CASE WHEN transportations.hazardous THEN 'true' ELSE 'false' END) COLLATE \"C\"");

inline constexpr registry::Field<std::string> kShipment{
    "shipment_id", "transportations.shipment_id", "UUID"};

inline constexpr registry::Field<std::string> kItem{"item_id", "transportations.item_id", "UUID"};

// Каталог API: доступные операции и источники отображаемых значений.
inline constexpr std::array<registry::RegistryField, 9> kFields{{
    {.Field = kId, .ValueSql = "transportations.id", .Visible = false},
    {.Field = kNumber, .ValueSql = "transportations.container_number", .FoldValues = true},
    {.Field = kType, .ValueSql = "transportations.type"},
    {.Field = kContainerType, .ValueSql = "COALESCE(transportations.container_type_code, '')"},
    {.Field = kOwnership, .ValueSql = "transportations.container_ownership"},
    {.Field = kHazardous, .ValueSql = "transportations.hazardous"},
    {.Field = kShipment,
        .ValueSql = "transportations.shipment_id",
        .Sortable = false,
        .Lookup = "shipments"},
    {.Field = kItem, .ValueSql = "transportations.item_id", .Sortable = false, .Visible = false},
    {.Field = kIncompleteContainer,
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
    route_registry::ApplyGroups(query, request.route_filters);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kRegistryFilterValues, &BuildRegistryFilters};

}  // namespace transportations::registry_query

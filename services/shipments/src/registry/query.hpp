#pragma once
#include <openapi/models.hpp>
#include <platform/registry_page.hpp>
#include <registry/route.hpp>
#include <shipments/sql_queries.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace shipments::registry_query {
namespace registry = platform::registry;

namespace registry_fields {

// SQL-поля: выражения фильтрации и, при необходимости, отдельный порядок сортировки.
using Date = userver::utils::datetime::Date;

inline constexpr registry::Field<boost::uuids::uuid> kId{"id", "shipments.id", "UUID", true};

inline constexpr registry::Field<boost::uuids::uuid> kOrganization{
    "organization_id", "shipments.organization_id", "UUID"};

inline constexpr auto kNumber =
    registry::Field<std::string>{"number", "lower(shipments.number) COLLATE \"C\""}.SortedAs(
        "lower(shipments.number) COLLATE public.registry_number_order");

inline constexpr registry::Field<std::string> kCargo{
    "cargo_description", "lower(COALESCE(shipments.cargo_description, '')) COLLATE \"C\""};

inline constexpr registry::Field<boost::uuids::uuid> kCustomer{
    "customer_id", "shipments.customer_id", "UUID"};

inline constexpr auto kStatus = registry::Field<std::string>{"status", "shipments.status"}.SortedAs(
    "lower(shipments.status) COLLATE \"C\"");

inline constexpr registry::Field<Date> kDate{"shipment_date", "shipments.shipment_date", "DATE"};

// Каталог API: доступные операции и источники отображаемых значений.
inline constexpr std::array<registry::RegistryField, 6> kFields{{
    {.Field = kId, .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .ValueSql = "number", .FoldValues = true},
    {.Field = kCargo, .ValueSql = "COALESCE(shipments.cargo_description, '')", .FoldValues = true},
    {.Field = kCustomer, .ValueSql = "customer_id", .Sortable = false, .Lookup = "counterparties"},
    {.Field = kStatus, .ValueSql = "status"},
    {.Field = kDate, .ValueSql = "shipment_date"},
}};

inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(const models::ListShipmentsRequest& request,
    const boost::uuids::uuid& organization,
    const std::optional<std::string>& exclude = std::nullopt) {
    const auto order = registry::RegistryOrderBy(registry_fields::kFields, request.sort);
    registry::QueryBuilder query{sql::kListShipments, order, request.descending};
    query.WhereEqual(registry_fields::kOrganization, organization);
    registry::ApplyFilters(query, registry_fields::kFields, request.filters, exclude);
    route_registry::ApplyGroups(query, request.route_filters);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kRegistryFilterValues, &BuildRegistryFilters};

}  // namespace shipments::registry_query

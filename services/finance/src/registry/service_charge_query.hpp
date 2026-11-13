#pragma once
#include <finance/sql_queries.hpp>
#include <helpers/service_charges.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <models/service_charge_registry_row.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>
#include <userver/utils/text.hpp>
#include <userver/utils/text_light.hpp>

namespace finance::registry_query::service_charges {
namespace pg = userver::storages::postgres;
namespace registry = platform::registry;

namespace fields {
using Date = userver::utils::datetime::Date;
inline constexpr registry::Field<std::string> kId{"id", "charge.id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "charge.organization_id", "UUID"};
inline constexpr registry::Field<std::string> kLabel{"label_name",
    "lower(charge.label_name COLLATE public.registry_number_order) COLLATE "
    "public.registry_number_order"};
inline constexpr registry::Field<std::string> kCounterparty{
    "counterparty_id", "charge.counterparty_id", "UUID"};
inline constexpr registry::Field<std::string> kDirection{"direction", "charge.direction"};
inline constexpr registry::Field<Date> kDate{"service_date", "charge.service_date", "DATE"};
inline constexpr registry::Field<std::string> kCurrency{"currency", "charge.currency"};
inline constexpr auto kInvoice =
    registry::Field<std::string>{"invoice_number",
        "lower(invoice_number COLLATE public.registry_number_order) COLLATE "
        "public.registry_number_order"}
        .SortedAs("COALESCE(lower(invoice_number COLLATE public.registry_number_order), '') "
                  "COLLATE public.registry_number_order");
inline constexpr registry::Field<std::string> kShipment{
    "shipment_id", "charge.shipment_id", "UUID"};
inline constexpr registry::Field<std::string> kTransportation{
    "transportation_id", "charge.transportation_id", "UUID"};
inline constexpr auto kArchived =
    registry::Field<bool>{"archived", "charge.archived", "BOOLEAN"}.SortedAs(
        "(CASE WHEN charge.archived THEN '1' ELSE '0' END) COLLATE public.registry_number_order");
inline constexpr registry::Field<std::int64_t> kNet{"net_minor", "charge.net_minor", "BIGINT"};
inline constexpr registry::Field<std::int64_t> kVat{"vat_minor", "charge.vat_minor", "BIGINT"};
inline constexpr registry::Field<std::int64_t> kTotal{
    "total_minor", "charge.total_minor", "BIGINT"};
inline constexpr std::array<registry::RegistryField, 13> kFields{{
    {.Field = kId, .ValueSql = "charge.id", .Visible = false},
    {.Field = kLabel, .ValueSql = "charge.label_name", .FoldValues = true},
    {.Field = kCounterparty, .ValueSql = "charge.counterparty_name", .Sortable = false},
    {.Field = kDirection,
        .ValueSql = "CASE charge.direction WHEN 'income' THEN 'Доход' ELSE 'Расход' END"},
    {.Field = kDate, .ValueSql = "charge.service_date"},
    {.Field = kCurrency, .ValueSql = "charge.currency"},
    {.Field = kInvoice, .ValueSql = "invoice_number", .FoldValues = true},
    {.Field = kShipment,
        .ValueSql = "charge.shipment_id",
        .Sortable = false,
        .Lookup = "shipments"},
    {.Field = kTransportation,
        .ValueSql = "charge.transportation_id",
        .Sortable = false,
        .Lookup = "transportations"},
    {.Field = kArchived, .ValueSql = "charge.archived"},
    {.Field = kNet, .ValueSql = "charge.net_minor", .Visible = false},
    {.Field = kVat, .ValueSql = "charge.vat_minor", .Visible = false},
    {.Field = kTotal, .ValueSql = "charge.total_minor", .Visible = false},
}};
}  // namespace fields

inline registry::QueryBuilder BuildRegistryFilters(
    const models::ServiceChargeRegistryRequest& request, const std::string& organization,
    const std::optional<std::string>& exclude = std::nullopt) {
    registry::QueryBuilder query{sql::kServiceChargeRegistry,
        registry::RegistryOrderBy(fields::kFields, request.sort), request.descending, fields::kId};
    query.WhereEqual(fields::kOrganization, organization);
    auto filters = request.filters;
    if (filters.has_value()) {
        for (auto& filter : filters.value()) {
            if (!registry::FindField(fields::kFields, filter.field).FoldValues)
                continue;
            if (filter.query.has_value())
                filter.query = userver::utils::text::ToLower(filter.query.value());
            if (filter.values.has_value())
                for (auto& value : filter.values.value())
                    value = userver::utils::text::ToLower(value);
        }
    }
    registry::ApplyFilters(query, fields::kFields, filters, exclude);
    return query;
}

inline const registry::Definition kRegistry{
    fields::kFields, sql::kServiceChargeRegistryFilterValues, &BuildRegistryFilters};
}  // namespace finance::registry_query::service_charges

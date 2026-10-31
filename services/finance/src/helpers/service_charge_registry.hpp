#pragma once
#include <helpers/service_charges.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>
#include <userver/utils/text.hpp>

namespace finance::helpers::service_charge_registry {
namespace registry = platform::registry;

struct RegistryRow {
    pgtypes::ServiceChargeV1 Charge;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

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
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "charge.id", .Visible = false},
    {.Field = kLabel,
        .Label = "Финансовая метка",
        .ValueSql = "charge.label_name",
        .FoldValues = true},
    {.Field = kCounterparty,
        .Label = "Контрагент",
        .ValueSql = "charge.counterparty_name",
        .Sortable = false},
    {.Field = kDirection,
        .Label = "Направление",
        .ValueSql = "CASE charge.direction WHEN 'income' THEN 'Доход' ELSE 'Расход' END"},
    {.Field = kDate, .Label = "Дата услуги", .ValueSql = "charge.service_date"},
    {.Field = kCurrency, .Label = "Валюта", .ValueSql = "charge.currency"},
    {.Field = kInvoice, .Label = "Счёт", .ValueSql = "invoice_number", .FoldValues = true},
    {.Field = kShipment,
        .Label = "Поставка",
        .ValueSql = "charge.shipment_id",
        .Sortable = false,
        .Lookup = "shipments"},
    {.Field = kTransportation,
        .Label = "Перевозка",
        .ValueSql = "charge.transportation_id",
        .Sortable = false,
        .Lookup = "transportations"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "charge.archived"},
    {.Field = kNet, .Label = "Без НДС", .ValueSql = "charge.net_minor", .Visible = false},
    {.Field = kVat, .Label = "НДС", .ValueSql = "charge.vat_minor", .Visible = false},
    {.Field = kTotal, .Label = "Итого", .ValueSql = "charge.total_minor", .Visible = false},
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
}  // namespace finance::helpers::service_charge_registry

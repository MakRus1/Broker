#pragma once
#include <helpers/database.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace sales::helpers {
namespace registry = platform::registry;
using Date = userver::utils::datetime::Date;
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr registry::Field<std::string> kCustomer{"customer_id", "customer_id", "UUID"};
inline constexpr registry::Field<std::string> kManager{"manager_subject", "manager_subject"};
inline constexpr registry::Field<std::string> kStage{"stage_id", "stage_id", "UUID"};
inline constexpr registry::Field<std::string> kSource{"source", "source"};
inline constexpr registry::Field<std::string> kAction{"next_action", "next_action"};
inline constexpr registry::Field<Date> kActionDate{"next_action_date", "next_action_date", "DATE"};
inline constexpr auto kNumber =
    registry::Field<std::string>{"number", "lower(number) COLLATE \"C\""}.SortedAs(
        "lower(number) COLLATE public.registry_number_order");
inline constexpr registry::Field<std::string> kStatus{"status", "status"};
inline constexpr registry::Field<std::string> kCargo{"cargo_description", "cargo_description"};
inline constexpr registry::Field<Date> kReady{"ready_date", "ready_date", "DATE"};
inline constexpr registry::Field<Date> kDelivery{
    "desired_delivery_date", "desired_delivery_date", "DATE"};
inline constexpr registry::Field<std::string> kShipment{"shipment_id", "shipment_id", "UUID"};
inline constexpr std::array<registry::RegistryField, 7> kCardFields{{
    {.Field = kId, .ValueSql = "id", .Visible = false},
    {.Field = kCustomer, .ValueSql = "customer_id", .Sortable = false, .Lookup = "counterparties"},
    {.Field = kManager, .ValueSql = "manager_subject", .Sortable = false, .Lookup = "employees"},
    {.Field = kStage, .ValueSql = R"((SELECT stage->>'name'
            FROM sales_settings, jsonb_array_elements(stages) AS stage
            WHERE sales_settings.organization_id = sales_cards.organization_id
                AND stage->>'id' = sales_cards.stage_id::TEXT))"},
    {.Field = kSource, .ValueSql = "source"},
    {.Field = kAction, .ValueSql = "next_action"},
    {.Field = kActionDate, .ValueSql = "next_action_date"},
}};
inline constexpr std::array<registry::RegistryField, 8> kInquiryFields{{
    {.Field = kId, .ValueSql = "id", .Visible = false},
    {.Field = kCustomer, .ValueSql = "customer_id", .Sortable = false, .Lookup = "counterparties"},
    {.Field = kManager, .ValueSql = "manager_subject", .Sortable = false, .Lookup = "employees"},
    {.Field = kNumber, .ValueSql = "number"},
    {.Field = kStatus, .ValueSql = "status"},
    {.Field = kCargo, .ValueSql = "cargo_description"},
    {.Field = kReady, .ValueSql = "ready_date"},
    {.Field = kDelivery, .ValueSql = "desired_delivery_date"},
}};

inline registry::QueryBuilder CardsFilters(const models::ListSalesRequest& body,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    registry::QueryBuilder query{
        sql::kListCards, registry::RegistryOrderBy(kCardFields, body.sort), body.descending};
    query.WhereEqual(kOrganization, organization);
    if (body.customer_id.has_value())
        query.WhereEqual(kCustomer, body.customer_id.value());
    registry::ApplyFilters(query, kCardFields, body.filters, exclude);
    return query;
}

inline const registry::Definition kCardsRegistry{
    kCardFields, sql::kCardsFilterValues, &CardsFilters};

inline registry::QueryBuilder InquiriesFilters(const models::ListSalesRequest& body,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    registry::QueryBuilder query{
        sql::kListInquiries, registry::RegistryOrderBy(kInquiryFields, body.sort), body.descending};
    query.WhereEqual(kOrganization, organization);
    if (body.customer_id.has_value())
        query.WhereEqual(kCustomer, body.customer_id.value());
    if (body.shipment_id.has_value())
        query.WhereEqual(kShipment, body.shipment_id.value());
    registry::ApplyFilters(query, kInquiryFields, body.filters, exclude);
    return query;
}

inline const registry::Definition kInquiriesRegistry{
    kInquiryFields, sql::kInquiriesFilterValues, &InquiriesFilters};

struct RegistryRow {
    json::Value Document;
    std::string TextKey;
    std::optional<Date> DateKey;
};

}  // namespace sales::helpers

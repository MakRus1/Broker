#pragma once
#include <helpers/storage.hpp>
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
    {.Field = kStage, .ValueSql = "stage_id"},
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

template <typename Model> struct Page {
    std::vector<Model> Items;
    std::optional<models::SalesRegistryCursor> Cursor;
};

template <typename Model>
Page<Model> List(const Deps& deps,
    const registry::Definition<models::ListSalesRequest, std::string>& definition,
    const models::ListSalesRequest& body, const std::string& organization) {
    const auto scope = registry::CursorScope(body, organization);
    const bool date = registry::RegistryOrderBy(definition.Fields, body.sort).CursorType ==
        registry::CursorKeyType::Date;
    if (body.cursor.has_value() &&
        (body.cursor->scope != scope || date != body.cursor->date_value.has_value() ||
            date == body.cursor->text_value.has_value()))
        throw ApiError<400>{"invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
    const auto query = registry::BuildPage(definition, body, organization);
    auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
            .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
    const bool more = rows.size() > static_cast<std::size_t>(body.limit);
    if (more)
        rows.pop_back();
    Page<Model> page;
    for (const auto& row : rows)
        page.Items.push_back(models::ParseModelJson(row.Document, std::type_identity<Model>{}));
    if (more)
        page.Cursor =
            models::SalesRegistryCursor{.date_value = date ? rows.back().DateKey : std::nullopt,
                .id = page.Items.back().id,
                .scope = scope,
                .text_value = date ? std::nullopt : std::optional{rows.back().TextKey}};
    return page;
}
}  // namespace sales::helpers

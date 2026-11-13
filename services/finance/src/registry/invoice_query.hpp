#pragma once
#include <converters/invoice.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/invoices.hpp>
#include <models/invoice_registry_row.hpp>
#include <models/pg/invoice_raw.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>

namespace finance::registry_query::invoices {
namespace pg = userver::storages::postgres;
namespace registry = platform::registry;

namespace registry_fields {

// Поля SQL и каталог API определяются здесь; общий конструктор добавляет фильтры и курсор.
using Date = userver::utils::datetime::Date;
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr auto kNumber =
    registry::Field<std::string>{"number", "lower(number) COLLATE \"C\""}.SortedAs(
        "lower(number) COLLATE public.registry_number_order");
inline constexpr registry::Field<std::string> kOwnCompany{
    "own_company_id", "own_company_id", "UUID"};
inline constexpr registry::Field<std::string> kCounterparty{
    "counterparty_id", "counterparty_id", "UUID"};
inline constexpr registry::Field<Date> kDate{"invoice_date", "invoice_date", "DATE"};
inline constexpr auto kDueDate = registry::Field<Date>{"due_date", "due_date", "DATE"}.SortedAs(
    "COALESCE(due_date, DATE '9999-12-31')");
inline constexpr registry::Field<std::string> kContract{"contract_id", "contract_id", "UUID"};
inline constexpr registry::Field<std::string> kDirection{"direction", "direction"};
inline constexpr registry::Field<std::string> kCurrency{"currency", "currency"};
inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr registry::Field<std::int64_t> kTotal{
    "total_minor", "invoice.total_minor", "BIGINT"};
inline constexpr registry::Field<std::int64_t> kPaid{
    "paid_minor", "payment_total.paid_minor", "BIGINT"};
inline constexpr registry::Field<std::int64_t> kOutstanding{
    "outstanding_minor", "settlement.outstanding_minor", "BIGINT"};
inline constexpr registry::Field<std::string> kPaymentStatus{
    "payment_status", "settlement.payment_status"};
inline constexpr registry::Field<bool> kOverdue{
    "overdue", "(settlement.overdue_days > 0)", "BOOLEAN"};
inline constexpr registry::Field<int> kOverdueDays{
    "overdue_days", "settlement.overdue_days", "INTEGER"};
inline constexpr std::array<registry::RegistryField, 15> kFields{{
    {.Field = kId, .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .ValueSql = "number", .FoldValues = true},
    {.Field = kOwnCompany,
        .ValueSql = "own_company_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kCounterparty,
        .ValueSql = "counterparty_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kDate, .ValueSql = "invoice_date"},
    {.Field = kDueDate, .ValueSql = "due_date"},
    {.Field = kContract, .ValueSql = "contract_id", .Sortable = false, .Visible = false},
    {.Field = kCurrency, .ValueSql = "currency"},
    {.Field = kArchived, .ValueSql = "archived"},
    {.Field = kTotal, .ValueSql = "invoice.total_minor", .Visible = false},
    {.Field = kPaid, .ValueSql = "payment_total.paid_minor", .Visible = false},
    {.Field = kOutstanding, .ValueSql = "settlement.outstanding_minor", .Visible = false},
    {.Field = kPaymentStatus,
        .ValueSql = "CASE settlement.payment_status WHEN 'paid' THEN 'Оплачен' WHEN 'partial' THEN "
                    "'Частично оплачен' ELSE 'Не оплачен' END"},
    {.Field = kOverdueDays, .ValueSql = "settlement.overdue_days", .Visible = false},
    {.Field = kOverdue, .ValueSql = "(settlement.overdue_days > 0)", .Sortable = false},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(const models::ListInvoicesRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{
        sql::kListInvoices, registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    query.WhereEqual(kDirection, models::EnumValue(request.direction));
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kInvoiceRegistryFilterValues, &BuildRegistryFilters};

}  // namespace finance::registry_query::invoices

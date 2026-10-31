#pragma once
#include <helpers/invoices.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>

namespace finance::helpers::invoice_registry {
namespace registry = platform::registry;

struct RegistryRow {
    pgtypes::InvoiceV1 Invoice;
    std::int64_t PaidMinor;
    std::int64_t OutstandingMinor;
    std::string PaymentStatus;
    int OverdueDays;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

inline std::vector<models::InvoiceRegistryItem> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::InvoiceRegistryItem> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back({.invoice = ::finance::helpers::Convert(row.Invoice),
            .outstanding_minor = row.OutstandingMinor,
            .overdue_days = row.OverdueDays,
            .paid_minor = row.PaidMinor,
            .payment_status =
                models::ParseEnumScalar(row.PaymentStatus, models::InvoicePaymentStatus{})});
    return result;
}

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
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .Label = "Номер счёта", .ValueSql = "number", .FoldValues = true},
    {.Field = kOwnCompany,
        .Label = "Собственное юрлицо",
        .ValueSql = "own_company_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kCounterparty,
        .Label = "Контрагент",
        .ValueSql = "counterparty_id",
        .Sortable = false,
        .Lookup = "counterparties"},
    {.Field = kDate, .Label = "Дата счёта", .ValueSql = "invoice_date"},
    {.Field = kDueDate, .Label = "Плановая оплата", .ValueSql = "due_date"},
    {.Field = kContract,
        .Label = "Договор",
        .ValueSql = "contract_id",
        .Sortable = false,
        .Visible = false},
    {.Field = kCurrency, .Label = "Валюта", .ValueSql = "currency"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "archived"},
    {.Field = kTotal, .Label = "Сумма с НДС", .ValueSql = "invoice.total_minor", .Visible = false},
    {.Field = kPaid, .Label = "Оплачено", .ValueSql = "payment_total.paid_minor", .Visible = false},
    {.Field = kOutstanding,
        .Label = "Осталось оплатить",
        .ValueSql = "settlement.outstanding_minor",
        .Visible = false},
    {.Field = kPaymentStatus,
        .Label = "Оплата",
        .ValueSql = "CASE settlement.payment_status WHEN 'paid' THEN 'Оплачен' WHEN 'partial' THEN "
                    "'Частично оплачен' ELSE 'Не оплачен' END"},
    {.Field = kOverdueDays,
        .Label = "Дни просрочки",
        .ValueSql = "settlement.overdue_days",
        .Visible = false},
    {.Field = kOverdue,
        .Label = "Просрочен",
        .ValueSql = "(settlement.overdue_days > 0)",
        .Sortable = false},
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

}  // namespace finance::helpers::invoice_registry

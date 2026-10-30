#pragma once
#include <helpers/invoices.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>

namespace finance::helpers::invoice_registry {
namespace registry = platform::registry;

struct RegistryRow {
    pgtypes::InvoiceV1 Invoice;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

inline std::vector<models::Invoice> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::Invoice> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(::finance::helpers::Convert(row.Invoice));
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
inline constexpr std::array<registry::RegistryField, 9> kFields{{
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

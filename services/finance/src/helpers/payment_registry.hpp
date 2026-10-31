#pragma once
#include <helpers/payments.hpp>
#include <platform/registry.hpp>
#include <platform/registry_page.hpp>

namespace finance::helpers::payment_registry {
namespace registry = platform::registry;

struct RegistryRow {
    pgtypes::PaymentV1 Payment;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

inline std::vector<models::Payment> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::Payment> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(::finance::helpers::Convert(row.Payment));
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
inline constexpr registry::Field<Date> kDate{"payment_date", "payment_date", "DATE"};
inline constexpr registry::Field<std::string> kKind{"kind", "kind"};
inline constexpr registry::Field<std::string> kCurrency{"currency", "currency"};
inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr registry::Field<bool> kHasUnallocated{"has_unallocated",
    "(invoice.amount_minor > (SELECT COALESCE(SUM(allocation.amount_minor), 0) "
    "FROM payment_allocations AS allocation "
    "WHERE allocation.organization_id = invoice.organization_id "
    "AND allocation.payment_id = invoice.id))",
    "BOOLEAN"};
inline constexpr std::array<registry::RegistryField, 8> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .Label = "Номер платежа", .ValueSql = "number", .FoldValues = true},
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
    {.Field = kDate, .Label = "Дата платежа", .ValueSql = "payment_date"},
    {.Field = kCurrency, .Label = "Валюта", .ValueSql = "currency"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "archived"},
    {.Field = kHasUnallocated,
        .Label = "Есть нераспределённый остаток",
        .ValueSql = kHasUnallocated.Expression,
        .Sortable = false,
        .Visible = false},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(const models::ListPaymentsRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{
        sql::kListPayments, registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    query.WhereEqual(kKind, models::EnumValue(request.kind.value_or(models::PaymentKind::Receipt)));
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kPaymentRegistryFilterValues, &BuildRegistryFilters};

}  // namespace finance::helpers::payment_registry

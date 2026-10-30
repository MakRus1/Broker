#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_page.hpp>

namespace contracts::helpers {
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
inline constexpr registry::Field<Date> kDate{"contract_date", "contract_date", "DATE"};
inline constexpr auto kValidUntil =
    registry::Field<Date>{"valid_until", "valid_until", "DATE"}.SortedAs(
        "COALESCE(valid_until, DATE '9999-12-31')");
inline constexpr registry::Field<std::string> kCurrency{"currency", "currency"};
inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr std::array<registry::RegistryField, 8> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kNumber, .Label = "Номер договора", .ValueSql = "number", .FoldValues = true},
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
    {.Field = kDate, .Label = "Дата договора", .ValueSql = "contract_date"},
    {.Field = kValidUntil, .Label = "Действует до", .ValueSql = "valid_until"},
    {.Field = kCurrency, .Label = "Валюта", .ValueSql = "currency"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "archived"},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

// Общие условия страницы и списка вариантов: организация и применённые фильтры.
inline registry::QueryBuilder BuildRegistryFilters(const models::ListContractsRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{
        sql::kListContracts, registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kRegistryFilterValues, &BuildRegistryFilters};

}  // namespace contracts::helpers

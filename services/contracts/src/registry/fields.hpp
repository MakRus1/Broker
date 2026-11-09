#pragma once
#include <array>
#include <platform/registry.hpp>

namespace contracts {
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
    {.Field = kDate, .ValueSql = "contract_date"},
    {.Field = kValidUntil, .ValueSql = "valid_until"},
    {.Field = kCurrency, .ValueSql = "currency"},
    {.Field = kArchived, .ValueSql = "archived"},
}};
}  // namespace registry_fields

}  // namespace contracts

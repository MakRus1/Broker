#pragma once
#include <array>
#include <platform/registry_filters.hpp>

namespace counterparties {
namespace registry = platform::registry;

namespace registry_fields {

// SQL-поля: выражения фильтрации и, при необходимости, отдельный порядок сортировки.
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};

inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};

inline constexpr registry::Field<std::string> kName{"name", "lower(name) COLLATE \"C\""};

inline constexpr auto kCountry =
    registry::Field<std::string>{"country_code", "COALESCE(country_code, '')"}.SortedAs(
        "lower(COALESCE(country_code, '')) COLLATE \"C\"");

inline constexpr registry::Field<std::string> kTaxId{
    "tax_id", "COALESCE(tax_id, '') COLLATE \"C\""};

inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");

inline constexpr registry::Field<std::string> kSearch{"search",
    "lower(name || ' ' || COALESCE(legal_name, '') || ' ' || COALESCE(tax_id, '')) COLLATE \"C\""};

inline constexpr registry::Field<std::string> kRoles{"roles", "roles"};

inline constexpr registry::Field<bool> kOwnCompany{"own_company", "own_company", "BOOLEAN"};

// Каталог API: доступные операции и источники отображаемых значений.
inline constexpr std::array<registry::RegistryField, 8> kFields{{
    {.Field = kId, .ValueSql = "id", .Visible = false},
    {.Field = kName, .ValueSql = "name", .FoldValues = true},
    {.Field = kCountry, .ValueSql = "COALESCE(country_code, '')"},
    {.Field = kTaxId, .ValueSql = "COALESCE(tax_id, '')"},
    {.Field = kArchived, .ValueSql = "archived"},
    {.Field = kOwnCompany, .ValueSql = "own_company", .Sortable = false},
    {.Field = kRoles, .ValueSql = "roles", .Sortable = false, .Array = true},
    {.Field = kSearch,
        .ValueSql = kSearch.Expression,
        .Sortable = false,
        .FoldValues = true,
        .Visible = false},
}};

}  // namespace registry_fields

}  // namespace counterparties

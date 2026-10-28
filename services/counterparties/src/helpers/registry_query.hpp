#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_filters.hpp>

namespace counterparties::helpers {
namespace registry = platform::registry;

namespace registry_fields {
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
inline constexpr std::array<registry::RegistryField, 7> kFields{{
    {.Field = kId, .Label = "Идентификатор", .ValueSql = "id", .Visible = false},
    {.Field = kName, .Label = "Наименование", .ValueSql = "name", .FoldValues = true},
    {.Field = kCountry, .Label = "Страна", .ValueSql = "COALESCE(country_code, '')"},
    {.Field = kTaxId, .Label = "Налоговый номер", .ValueSql = "COALESCE(tax_id, '')"},
    {.Field = kArchived, .Label = "Состояние", .ValueSql = "archived"},
    {.Field = kRoles, .Label = "Роли", .ValueSql = "roles", .Sortable = false, .Array = true},
    {.Field = kSearch,
        .Label = "Поиск контрагента",
        .ValueSql = kSearch.Expression,
        .Sortable = false,
        .FoldValues = true,
        .Visible = false},
}};
inline constexpr auto kSortableFields = registry::SortableFields(kFields);
}  // namespace registry_fields

inline registry::QueryBuilder BuildRegistryFilters(const models::ListCounterpartiesRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListCounterparties,
        registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListCounterpartiesRequest& request, const std::string& organization) {
    using namespace registry_fields;
    auto query = BuildRegistryFilters(request, organization);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}

inline registry::BuiltQuery BuildRegistryValues(const models::ListCounterpartiesRequest& request,
    const std::string& organization, const std::string& field,
    const std::optional<std::string>& search, const std::optional<std::string>& after, int limit,
    const std::optional<std::vector<std::string>>& candidates = std::nullopt) {
    const auto& definition = registry::FindField(registry_fields::kFields, field);
    auto builder = BuildRegistryFilters(request, organization, field);
    registry::ApplyCandidates(builder, definition, candidates);
    return registry::BuildValues(
        std::move(builder), sql::kRegistryFilterValues, definition, search, after, limit + 1);
}
}  // namespace counterparties::helpers

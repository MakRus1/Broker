#pragma once
#include <helpers/registry.hpp>
#include <platform/registry_query.hpp>

namespace counterparties::helpers {
namespace registry = platform::registry;

namespace registry_fields {
inline constexpr registry::Field<std::string> kId{"id", "id", "UUID", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "organization_id", "UUID"};
inline constexpr registry::Field<std::string> kName{"name", "lower(name) COLLATE \"C\""};
inline constexpr auto kCountry =
    registry::Field<std::string>{"country_code", "country_code"}.SortedAs(
        "lower(COALESCE(country_code, '')) COLLATE \"C\"");
inline constexpr registry::Field<std::string> kTaxId{
    "tax_id", "COALESCE(tax_id, '') COLLATE \"C\""};
inline constexpr auto kArchived = registry::Field<bool>{"archived", "archived", "BOOLEAN"}.SortedAs(
    "(CASE WHEN archived THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr registry::Field<std::string> kSearch{"query",
    "lower(name || ' ' || COALESCE(legal_name, '') || ' ' || COALESCE(tax_id, '')) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kRoles{"roles", "roles"};
inline constexpr std::array<registry::FieldDefinition, 5> kSortableFields{
    kId, kName, kCountry, kTaxId, kArchived};
}  // namespace registry_fields

inline registry::BuiltQuery BuildRegistryQuery(
    const models::ListCounterpartiesRequest& request, const std::string& organization) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListCounterparties,
        registry::OrderBy(kSortableFields, models::EnumValue(request.sort)), request.descending};
    query.WhereEqual(kOrganization, organization);
    query.WhereContains(kSearch, request.query);
    std::optional<std::string> role;
    if (request.role.has_value())
        role = models::EnumValue(request.role.value());
    query.WhereArrayContains(kRoles, role);
    if (request.status != models::ListCounterpartiesRequest_status::All)
        query.WhereEqual(
            kArchived, request.status == models::ListCounterpartiesRequest_status::Archived);
    query.WhereEqual(kCountry, request.country_code);
    query.WhereAny(kId, request.ids);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}
}  // namespace counterparties::helpers

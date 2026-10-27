#pragma once
#include <helpers/access.hpp>
#include <helpers/converters.hpp>
#include <platform/registry_query.hpp>

namespace organizations::helpers {
namespace registry = platform::registry;

namespace organization_fields {
inline constexpr registry::Field<std::string> kId{"id", "o.id", "UUID", true};
inline constexpr registry::Field<std::string> kName{"name", "lower(o.name) COLLATE \"C\""};
inline constexpr auto kEnabled = registry::Field<bool>{"enabled", "o.enabled", "BOOLEAN"}.SortedAs(
    "(CASE WHEN o.enabled THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr std::array<registry::FieldDefinition, 3> kSorts{kId, kName, kEnabled};
}  // namespace organization_fields

namespace member_fields {
inline constexpr registry::Field<std::string> kSubject{"subject", "m.subject", "TEXT", true};
inline constexpr registry::Field<std::string> kOrganization{
    "organization_id", "m.organization_id", "UUID"};
inline constexpr registry::Field<std::string> kName{
    "display_name", "lower(m.display_name) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kUsername{
    "username", "lower(m.username) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kEmail{"email", "lower(m.email) COLLATE \"C\""};
inline constexpr auto kRole = registry::Field<std::string>{"role", "m.role"}.SortedAs(
    "lower(COALESCE(r.name, m.role)) COLLATE \"C\"");
inline constexpr auto kEnabled = registry::Field<bool>{"enabled", "m.enabled", "BOOLEAN"}.SortedAs(
    "(CASE WHEN m.enabled THEN '1' ELSE '0' END) COLLATE \"C\"");
inline constexpr registry::Field<std::string> kSearch{
    "query", "lower(m.display_name || ' ' || m.username || ' ' || m.email) COLLATE \"C\""};
inline constexpr std::array<registry::FieldDefinition, 6> kSorts{
    kSubject, kName, kUsername, kEmail, kRole, kEnabled};
}  // namespace member_fields

template <typename Request> registry::BuiltQuery OrganizationQuery(const Request& request) {
    using namespace organization_fields;
    registry::QueryBuilder query{sql::kListOrganizations,
        registry::OrderBy(kSorts, EnumValue(request.sort)), request.descending, kId};
    query.WhereContains(kName, request.query);
    query.WhereEqual(kEnabled, request.enabled);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}

template <typename Request> registry::BuiltQuery MemberQuery(const Request& request) {
    using namespace member_fields;
    registry::QueryBuilder query{sql::kListMembers,
        registry::OrderBy(kSorts, EnumValue(request.sort)), request.descending, kSubject};
    query.WhereEqual(kOrganization, request.organization_id);
    query.WhereContains(kSearch, request.query);
    query.WhereEqual(kEnabled, request.enabled);
    query.WhereEqual(kRole, request.role);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}
}  // namespace organizations::helpers

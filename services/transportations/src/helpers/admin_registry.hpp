#pragma once
#include <helpers/database.hpp>
#include <openapi/models.hpp>
#include <platform/registry_query.hpp>

namespace transportations::helpers {
namespace registry = platform::registry;

namespace directory_fields {
inline constexpr registry::Field<std::string> kId{"esr_code", "esr_code", "TEXT", true};
inline constexpr registry::Field<std::string> kName{"name", "lower(name) COLLATE \"C\""};
inline constexpr registry::Field<std::string> kCountry{"country_code", "country_code"};
inline constexpr registry::Field<std::string> kSearch{
    "query", "lower(name || ' ' || esr_code) COLLATE \"C\""};
inline constexpr registry::Field<bool> kArchived{"archived", "archived", "BOOLEAN"};
inline constexpr std::array<registry::FieldDefinition, 3> kSorts{kId, kName, kCountry};
}  // namespace directory_fields

template <typename Request> registry::BuiltQuery DirectoryQuery(const Request& request) {
    using namespace directory_fields;
    registry::QueryBuilder query{sql::kListRailwayStations,
        registry::OrderBy(kSorts, EnumValue(request.sort)), request.descending, kId};
    query.WhereEqual(kArchived, false);
    query.WhereContains(kSearch, request.query);
    query.WhereEqual(kCountry, request.country_code);
    query.After(
        request.cursor.has_value() ? std::optional{request.cursor.value().id} : request.after,
        request.cursor.has_value() ? request.cursor.value().value : "");
    return std::move(query).Build(request.limit + 1);
}
}  // namespace transportations::helpers

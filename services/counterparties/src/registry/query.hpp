#pragma once
#include <counterparties/sql_queries.hpp>
#include <openapi/models.hpp>
#include <platform/registry_page.hpp>
#include <registry/fields.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace counterparties::registry_query {
inline registry::QueryBuilder BuildRegistryFilters(const models::ListCounterpartiesRequest& request,
    const std::string& organization, const std::optional<std::string>& exclude = std::nullopt) {
    using namespace registry_fields;
    registry::QueryBuilder query{sql::kListCounterparties,
        registry::RegistryOrderBy(kFields, request.sort), request.descending};
    query.WhereEqual(kOrganization, organization);
    registry::ApplyFilters(query, kFields, request.filters, exclude);
    return query;
}

inline const registry::Definition kRegistry{
    registry_fields::kFields, sql::kRegistryFilterValues, &BuildRegistryFilters};

}  // namespace counterparties::registry_query

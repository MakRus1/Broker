#pragma once
#include <models/registry_default_row.hpp>
#include <openapi/models.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

namespace organizations::converters {

inline models::RegistryDefault Convert(const models::RegistryDefaultRow& row) {
    return {
        .registry = row.Registry, .revision = row.Revision, .role = row.Role, .value = row.Value};
}

inline std::vector<models::RegistryDefault> ConvertRegistryDefaults(
    const userver::storages::postgres::ResultSet& rows) {
    std::vector<models::RegistryDefault> result;
    for (const auto& row :
        rows.AsSetOf<models::RegistryDefaultRow>(userver::storages::postgres::kRowTag))
        result.push_back(Convert(row));
    return result;
}

}  // namespace organizations::converters

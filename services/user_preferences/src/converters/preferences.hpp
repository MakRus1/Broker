#pragma once
#include <models/preference_row.hpp>
#include <openapi/models.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/result_set.hpp>

namespace user_preferences::converters {
namespace pg = userver::storages::postgres;

inline models::Preference Convert(const models::PreferenceRow& row) {
    return {.key = row.Key, .revision = row.Revision, .value = row.Value};
}

inline std::vector<models::Preference> Convert(const pg::ResultSet& rows) {
    std::vector<models::Preference> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<models::PreferenceRow>(pg::kRowTag))
        result.push_back(Convert(row));
    return result;
}

}  // namespace user_preferences::converters

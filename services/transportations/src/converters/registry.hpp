#pragma once
#include <models/pg/transportation_raw.hpp>
#include <openapi/models.hpp>

namespace transportations::converters {
struct RegistryRow {
    models::pg::TransportationRaw Transportation;
    std::string SortKey;
};

inline std::vector<models::pg::TransportationRaw> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::pg::TransportationRaw> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.Transportation);
    return result;
}
}  // namespace transportations::converters

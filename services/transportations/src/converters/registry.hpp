#pragma once
#include <models/transportation_registry_row.hpp>
#include <openapi/models.hpp>

namespace transportations::converters {

inline std::vector<models::pg::TransportationRaw> Convert(
    const std::vector<models::TransportationRegistryRow>& rows) {
    std::vector<models::pg::TransportationRaw> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.Transportation);
    return result;
}
}  // namespace transportations::converters

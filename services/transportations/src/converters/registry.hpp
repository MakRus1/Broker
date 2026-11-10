#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace transportations::converters {
struct RegistryRow {
    pgtypes::TransportationV4 Transportation;
    std::string SortKey;
};

inline std::vector<pgtypes::TransportationV4> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<pgtypes::TransportationV4> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.Transportation);
    return result;
}
}  // namespace transportations::converters

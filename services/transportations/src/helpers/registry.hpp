#pragma once
#include <helpers/database.hpp>
#include <openapi/models.hpp>
#include <platform/registry.hpp>
#include <postgresql/types.hpp>

namespace transportations::helpers {
struct RegistryRow {
    pgtypes::TransportationV4 Transportation;
    std::string SortKey;
};

using platform::registry::CursorScope;
using platform::registry::SearchPattern;

inline std::vector<pgtypes::TransportationV4> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<pgtypes::TransportationV4> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.Transportation);
    return result;
}
}  // namespace transportations::helpers

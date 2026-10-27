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

inline std::optional<std::vector<std::string>> Convert(
    const std::optional<std::vector<models::ListTransportationsRequest_types_item>>& values) {
    if (!values.has_value())
        return std::nullopt;
    std::vector<std::string> result;
    for (const auto value : values.value())
        result.push_back(models::EnumValue(value));
    return result;
}

inline std::vector<pgtypes::TransportationV4> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<pgtypes::TransportationV4> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.Transportation);
    return result;
}
}  // namespace transportations::helpers

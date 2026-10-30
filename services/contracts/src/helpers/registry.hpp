#pragma once
#include <helpers/contracts.hpp>
#include <platform/registry.hpp>

namespace contracts::helpers {
struct RegistryRow {
    pgtypes::ContractV1 Contract;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

inline std::vector<models::Contract> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::Contract> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Contract));
    return result;
}
}  // namespace contracts::helpers

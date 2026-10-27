#pragma once
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/registry.hpp>

namespace counterparties::helpers {
struct RegistryRow {
    pgtypes::CounterpartyV5 Counterparty;
    std::string SortKey;
};

inline std::vector<models::Counterparty> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::Counterparty> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Counterparty));
    return result;
}
}  // namespace counterparties::helpers

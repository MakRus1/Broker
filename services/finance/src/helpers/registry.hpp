#pragma once
#include <helpers/financial_labels.hpp>
#include <platform/registry.hpp>

namespace finance::helpers {
struct RegistryRow {
    pgtypes::FinancialLabelV1 Label;
    std::string SortKey;
};

inline std::vector<models::FinancialLabel> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<models::FinancialLabel> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Label));
    return result;
}
}  // namespace finance::helpers

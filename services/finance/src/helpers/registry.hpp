#pragma once
#include <helpers/financial_labels.hpp>
#include <models/pg/financial_label_raw.hpp>
#include <platform/registry.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace finance::helpers {
struct RegistryRow {
    models::pg::FinancialLabelRaw Label;
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

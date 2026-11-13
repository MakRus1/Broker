#pragma once
#include <models/pg/financial_label_raw.hpp>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct FinancialLabelRegistryRow {
    models::pg::FinancialLabelRaw Label;
    std::string SortKey;
};
}  // namespace finance::models

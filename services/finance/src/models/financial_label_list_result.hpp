#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct FinancialLabelListResult {
    std::vector<models::FinancialLabel> items;
    std::optional<models::FinancialLabelRegistryCursor> next_cursor;
};
}  // namespace finance::models

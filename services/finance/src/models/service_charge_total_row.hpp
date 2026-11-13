#pragma once
#include <cstdint>
#include <string>

namespace finance::models {
struct ServiceChargeTotalRow {
    std::int32_t Group;
    std::string Currency;
    std::string Income;
    std::string Expense;
};
}  // namespace finance::models

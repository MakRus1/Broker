#pragma once
#include <optional>
#include <string>

namespace finance::models {
struct ServiceChargeTotalRow {
    std::string Currency;
    std::optional<std::string> DirectIncome;
    std::optional<std::string> DirectExpense;
    std::optional<std::string> TransportationIncome;
    std::optional<std::string> TransportationExpense;
    std::string Income;
    std::string Expense;
};
}  // namespace finance::models

#pragma once
#include <openapi/models.hpp>

namespace contracts::models {
struct ContractListResult {
    std::vector<Contract> items;
    std::optional<ContractRegistryCursor> next_cursor;
};
}  // namespace contracts::models

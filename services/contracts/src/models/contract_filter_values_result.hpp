#pragma once
#include <openapi/models.hpp>

namespace contracts::models {
struct ContractFilterValuesResult {
    std::vector<RegistryField> fields;
    std::vector<RegistryValue> values;
    std::optional<std::string> next_after;
};
}  // namespace contracts::models

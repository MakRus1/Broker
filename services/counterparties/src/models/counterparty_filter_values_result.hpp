#pragma once
#include <openapi/models.hpp>

namespace counterparties::models {
struct CounterpartyFilterValuesResult {
    std::vector<RegistryField> fields;
    std::optional<std::string> next_after;
    std::vector<RegistryValue> values;
};
}  // namespace counterparties::models

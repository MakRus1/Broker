#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct FinancialLabelFilterValuesResult {
    std::vector<models::RegistryField> fields;
    std::optional<std::string> next_after;
    std::vector<models::RegistryValue> values;
};
}  // namespace finance::models

#pragma once
#include <openapi/models.hpp>

namespace sales::models {
template <typename Item> struct RegistryPage {
    std::vector<Item> items;
    std::optional<SalesRegistryCursor> next_cursor;
};

struct RegistryFilterValues {
    std::vector<RegistryField> fields;
    std::vector<RegistryValue> values;
    std::optional<std::string> next_after;
};
}  // namespace sales::models

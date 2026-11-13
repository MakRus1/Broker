#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct InvoiceListResult {
    std::vector<models::InvoiceRegistryItem> items;
    std::optional<models::InvoiceRegistryCursor> next_cursor;
};
}  // namespace finance::models

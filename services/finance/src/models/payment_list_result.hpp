#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct PaymentListResult {
    std::vector<models::Payment> items;
    std::optional<models::InvoiceRegistryCursor> next_cursor;
};
}  // namespace finance::models

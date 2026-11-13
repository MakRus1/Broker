#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct AvailablePayments {
    std::vector<models::PaymentAvailable> items;
    std::optional<std::string> next_after;
};
}  // namespace finance::models

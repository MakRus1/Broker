#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct PaymentInvoiceCandidatesResult {
    std::vector<models::PaymentInvoice> items;
    std::optional<std::string> next_after;
};
}  // namespace finance::models

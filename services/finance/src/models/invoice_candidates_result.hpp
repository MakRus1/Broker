#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct InvoiceCandidatesResult {
    std::vector<models::ServiceCharge> items;
    std::optional<std::string> next_after;
};
}  // namespace finance::models

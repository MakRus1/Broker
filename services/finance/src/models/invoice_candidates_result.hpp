#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct InvoiceCandidatesResult {
    std::vector<models::ServiceCharge> items;
    std::optional<boost::uuids::uuid> next_after;
};
}  // namespace finance::models

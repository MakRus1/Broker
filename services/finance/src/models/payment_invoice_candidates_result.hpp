#pragma once
#include <boost/uuid/uuid.hpp>
#include <openapi/models.hpp>

namespace finance::models {
struct PaymentInvoiceCandidatesResult {
    std::vector<models::PaymentInvoice> items;
    std::optional<boost::uuids::uuid> next_after;
};
}  // namespace finance::models

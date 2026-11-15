#pragma once
#include <boost/uuid/uuid.hpp>
#include <openapi/models.hpp>

namespace finance::models {
struct AvailablePayments {
    std::vector<models::PaymentAvailable> items;
    std::optional<boost::uuids::uuid> next_after;
};
}  // namespace finance::models

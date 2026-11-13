#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct ServiceChargeRegistryResult {
    std::vector<models::ServiceChargeInvoice> invoices;
    std::vector<models::ServiceCharge> items;
    std::optional<models::ServiceChargeRegistryCursor> next_cursor;
};
}  // namespace finance::models

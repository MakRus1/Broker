#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct ServiceChargeListResult {
    std::vector<models::ServiceChargeInvoice> invoices;
    std::vector<models::ServiceCharge> items;
    std::optional<std::string> next_after;
    models::ReportingSummary reporting;
    models::ServiceChargeSummary summary;
};
}  // namespace finance::models

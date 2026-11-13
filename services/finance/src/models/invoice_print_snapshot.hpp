#pragma once
#include <openapi/models.hpp>

namespace finance::models {
struct InvoicePrintSnapshot {
    InvoiceDetails Details;
    std::optional<InvoicePrintData> Cached;
};
}  // namespace finance::models

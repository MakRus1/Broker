#pragma once
#include <models/pg/invoice_raw.hpp>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct InvoiceRegistryRow {
    models::pg::InvoiceRaw Invoice;
    std::int64_t PaidMinor;
    std::int64_t OutstandingMinor;
    std::string PaymentStatus;
    int OverdueDays;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};
}  // namespace finance::models

#pragma once
#include <cstdint>
#include <models/pg/payment_raw.hpp>

namespace finance::models {
struct InvoicePaymentRow {
    pg::PaymentRaw Payment;
    std::int64_t AmountMinor;
};
}  // namespace finance::models

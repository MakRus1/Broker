#pragma once
#include <cstdint>
#include <models/pg/payment_raw.hpp>

namespace finance::models {
struct PaymentAvailableRow {
    pg::PaymentRaw Payment;
    std::int64_t AvailableMinor;
};
}  // namespace finance::models

#pragma once
#include <models/pg/payment_raw.hpp>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct PaymentRegistryRow {
    models::pg::PaymentRaw Payment;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};
}  // namespace finance::models

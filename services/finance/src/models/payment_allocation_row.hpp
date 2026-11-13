#pragma once
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct PaymentAllocationRow {
    boost::uuids::uuid InvoiceId;
    std::int64_t AmountMinor;
};
}  // namespace finance::models

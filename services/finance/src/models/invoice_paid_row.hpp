#pragma once

#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <userver/storages/postgres/io/uuid.hpp>

namespace finance::models {
struct InvoicePaidRow {
    boost::uuids::uuid InvoiceId;
    std::int64_t PaidMinor{};
};
}  // namespace finance::models

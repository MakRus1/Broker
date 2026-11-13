#pragma once
#include <cstdint>
#include <models/pg/invoice_raw.hpp>

namespace finance::models {
struct InvoiceBalanceRow {
    pg::InvoiceRaw Invoice;
    std::int64_t PaidMinor;
};
}  // namespace finance::models

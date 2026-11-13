#pragma once
#include <cstdint>
#include <string>

namespace finance::models {
struct ExchangeQuoteRow {
    std::string Currency;
    std::int64_t Value;
    std::int64_t Nominal;
};
}  // namespace finance::models

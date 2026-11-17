#pragma once

#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/date.hpp>

namespace finance::models {
struct CachedExchangeQuote {
    std::string Currency;
    std::int64_t Value{};
    std::int64_t Nominal{};
    userver::utils::datetime::Date EffectiveDate;
};
}  // namespace finance::models

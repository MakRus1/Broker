#pragma once
#include <models/cbr_exchange_rates.hpp>
#include <string>

namespace finance::converters {
models::CbrExchangeRates ParseCbrExchangeRates(
    const std::string& body, userver::utils::datetime::Date requested_date);
}  // namespace finance::converters

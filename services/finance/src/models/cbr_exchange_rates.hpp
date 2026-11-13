#pragma once
#include <models/exchange_quote.hpp>
#include <userver/utils/datetime/date.hpp>
#include <vector>

namespace finance::models {
struct CbrExchangeRates {
    userver::utils::datetime::Date EffectiveDate;
    std::vector<ExchangeQuote> Quotes;
};
}  // namespace finance::models

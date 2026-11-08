#pragma once
#include <cstdint>
#include <optional>
#include <platform/handler_error.hpp>

namespace platform {
struct ChargeAmounts {
    std::int64_t Net;
    std::int64_t Vat;
    std::int64_t Total;
};

inline std::int64_t RoundChargeRatio(std::int64_t numerator, std::int64_t denominator) {
    return (numerator + denominator / 2) / denominator;
}

inline ChargeAmounts CalculateChargeAmounts(std::int64_t quantity, std::int64_t price, bool gross,
    bool exempt, std::optional<std::int64_t> rate) {
    if (quantity < 1 || quantity > 10000000 || price < 0 || price > 100000000)
        throw http::HandlerError<400>{
            "invalid_amount", "Количество или цена превышают допустимый диапазон"};
    if (exempt == rate.has_value() || (rate && (*rate < 0 || *rate > 10000)))
        throw http::HandlerError<400>{"invalid_vat", "Укажите корректную ставку НДС"};
    const auto amount = RoundChargeRatio(quantity * price, 1000);
    const auto vat =
        RoundChargeRatio(amount * rate.value_or(0), gross ? 10000 + rate.value_or(0) : 10000);
    return gross ? ChargeAmounts{amount - vat, vat, amount}
                 : ChargeAmounts{amount, vat, amount + vat};
}
}  // namespace platform

#pragma once
#include <cstdint>
#include <models/errors.hpp>
#include <openapi/models.hpp>

namespace finance::helpers {
struct EntryAmounts {
    std::int64_t Net;
    std::int64_t Vat;
    std::int64_t Total;
};

// Ограничения API гарантируют, что произведения помещаются в int64_t.
// Округляем половину вверх один раз для суммы строки, затем для НДС.
inline std::int64_t RoundRatio(std::int64_t numerator, std::int64_t denominator) {
    return (numerator + denominator / 2) / denominator;
}

inline EntryAmounts CalculateAmounts(const models::SaveFinancialEntryRequest& value) {
    if (value.quantity_milli < 1 || value.quantity_milli > 10000000 || value.price_minor < 0 ||
        value.price_minor > 100000000)
        throw ApiError<400>{"invalid_amount", "Количество или цена превышают допустимый диапазон"};
    if ((value.vat_mode == models::VatMode::Exempt && value.vat_rate_basis_points.has_value()) ||
        (value.vat_mode == models::VatMode::Rate && !value.vat_rate_basis_points.has_value()))
        throw ApiError<400>{
            "invalid_vat", "Для ставки НДС укажите процент; для «Без НДС» оставьте его пустым"};
    const auto rate = value.vat_rate_basis_points.value_or(0);
    if (rate < 0 || rate > 10000)
        throw ApiError<400>{"invalid_vat", "Ставка НДС должна быть от 0 до 100%"};
    const auto amount = RoundRatio(value.quantity_milli * value.price_minor, 1000);
    if (value.price_mode == models::PriceMode::Gross) {
        const auto vat = RoundRatio(amount * rate, 10000 + rate);
        return {.Net = amount - vat, .Vat = vat, .Total = amount};
    }
    const auto vat = RoundRatio(amount * rate, 10000);
    return {.Net = amount, .Vat = vat, .Total = amount + vat};
}
}  // namespace finance::helpers

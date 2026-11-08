#pragma once
#include <openapi/models.hpp>
#include <platform/charge_amounts.hpp>

namespace finance::helpers {
using ServiceChargeAmounts = platform::ChargeAmounts;

inline std::int64_t RoundRatio(std::int64_t numerator, std::int64_t denominator) {
    return platform::RoundChargeRatio(numerator, denominator);
}

inline ServiceChargeAmounts CalculateAmounts(const models::SaveServiceChargeRequest& value) {
    return platform::CalculateChargeAmounts(value.quantity_milli, value.price_minor,
        value.price_mode == models::PriceMode::Gross, value.vat_mode == models::VatMode::Exempt,
        value.vat_rate_basis_points);
}
}  // namespace finance::helpers

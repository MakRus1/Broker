#pragma once
#include <helpers/converters/contracts.hpp>
#include <helpers/converters/counterparties.hpp>
#include <helpers/converters/finance.hpp>
#include <helpers/converters/financial_history.hpp>
#include <helpers/converters/invoices.hpp>
#include <helpers/converters/locations.hpp>
#include <helpers/converters/payments.hpp>
#include <helpers/converters/shipments.hpp>
#include <helpers/converters/transportations.hpp>
#include <utility>
#include <vector>

namespace web_bff {
template <typename T>
auto Convert(
    const std::vector<T>& values) -> std::vector<decltype(Convert(std::declval<const T&>()))> {
    std::vector<decltype(Convert(std::declval<const T&>()))> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert(value));
    return result;
}
}  // namespace web_bff

#pragma once
#include <converters/contracts.hpp>
#include <converters/counterparties.hpp>
#include <converters/finance.hpp>
#include <converters/financial_history.hpp>
#include <converters/invoices.hpp>
#include <converters/locations.hpp>
#include <converters/payments.hpp>
#include <converters/reconciliation.hpp>
#include <converters/shipments.hpp>
#include <converters/transportations.hpp>
#include <utility>
#include <vector>

namespace web_bff::converters {
template <typename T>
auto Convert(
    const std::vector<T>& values) -> std::vector<decltype(Convert(std::declval<const T&>()))> {
    std::vector<decltype(Convert(std::declval<const T&>()))> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert(value));
    return result;
}
}  // namespace web_bff::converters

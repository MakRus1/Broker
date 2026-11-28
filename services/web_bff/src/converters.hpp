#pragma once
#include <converters/contracts.hpp>
#include <converters/counterparties.hpp>
#include <converters/finance.hpp>
#include <converters/financial_history.hpp>
#include <converters/invoices.hpp>
#include <converters/items.hpp>
#include <converters/locations.hpp>
#include <converters/payments.hpp>
#include <converters/reconciliation.hpp>
#include <converters/shipments.hpp>
#include <converters/transportations.hpp>
#include <vector>

namespace web_bff::converters {
template <typename T> auto Convert(const std::vector<T>& values) {
    return ConvertItems(values, [](const auto& value) { return Convert(value); });
}
}  // namespace web_bff::converters

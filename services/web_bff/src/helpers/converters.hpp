#pragma once
#include <helpers/converters/counterparties.hpp>
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

#pragma once
#include <type_traits>
#include <vector>

namespace web_bff::converters {
template <typename T, typename Converter>
auto ConvertItems(const std::vector<T>& values, Converter convert) {
    std::vector<std::invoke_result_t<Converter, const T&>> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(convert(value));
    return result;
}
}  // namespace web_bff::converters

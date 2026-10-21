#pragma once
#include <openapi/models.hpp>
#include <platform/browser/session.hpp>
#include <userver/formats/json.hpp>

namespace web_bff {

template <typename T, typename U> T Convert(const U& value) {
    return models::ParseModelJson(
        userver::formats::json::ValueBuilder(value).ExtractValue(), std::type_identity<T>{});
}

template <typename T, typename U> std::vector<T> Convert(const std::vector<U>& values) {
    std::vector<T> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert<T>(value));
    return result;
}
}  // namespace web_bff

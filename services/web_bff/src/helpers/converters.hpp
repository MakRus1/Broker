#pragma once
#include <openapi/models.hpp>
#include <platform/browser/session.hpp>
#include <userver/formats/json.hpp>

namespace web_bff {

template <typename T, typename U> T Convert(const U& value) {
    return models::ParseModelJson(
        userver::formats::json::ValueBuilder(value).ExtractValue(), std::type_identity<T>{});
}
}  // namespace web_bff

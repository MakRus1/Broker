#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace transportations::helpers {
using Json = userver::formats::json::Value;

inline Json Document(const models::RouteSegment& value) {
    auto local = value;
    local.sea.reset();
    local.air.reset();
    return userver::formats::json::ValueBuilder(local).ExtractValue();
}

template <typename T> Json Document(const T& value) {
    return userver::formats::json::ValueBuilder(value).ExtractValue();
}

}  // namespace transportations::helpers

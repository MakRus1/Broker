#pragma once
#include <userver/formats/json.hpp>

namespace transportations::helpers {
using Json = userver::formats::json::Value;

template <typename T> Json Document(const T& value) {
    return userver::formats::json::ValueBuilder(value).ExtractValue();
}

}  // namespace transportations::helpers

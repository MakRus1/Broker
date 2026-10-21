#pragma once
#include <userver/formats/json.hpp>

namespace shipments::helpers {
using Json = userver::formats::json::Value;

template <typename T> Json Document(const T& value) {
    return userver::formats::json::ValueBuilder(value).ExtractValue();
}

}  // namespace shipments::helpers

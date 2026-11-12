#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace transportations::converters {
inline models::Transportation RestoreTransportation(const userver::formats::json::Value& snapshot) {
    // Старый технический номер не входит в текущую модель перевозки.
    userver::formats::json::ValueBuilder document{snapshot};
    document.Remove("number");
    return models::ParseModelJson(
        document.ExtractValue(), std::type_identity<models::Transportation>{});
}
}  // namespace transportations::converters

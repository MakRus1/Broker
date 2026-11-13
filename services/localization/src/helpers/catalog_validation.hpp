#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json/value.hpp>

namespace localization::helpers {
userver::formats::json::Value ValidateAndEncodeCatalog(
    const models::EditableCatalog& catalog, const userver::formats::json::Value& source);
}  // namespace localization::helpers

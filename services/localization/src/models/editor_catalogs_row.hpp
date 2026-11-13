#pragma once
#include <userver/formats/json/value.hpp>

namespace localization::models {
struct EditorCatalogsRow {
    userver::formats::json::Value Published;
    userver::formats::json::Value Inherited;
};
}  // namespace localization::models

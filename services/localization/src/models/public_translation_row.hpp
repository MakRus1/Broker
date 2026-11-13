#pragma once
#include <string>
#include <userver/formats/json/value.hpp>

namespace localization::models {
struct PublicTranslationRow {
    std::string Key;
    userver::formats::json::Value Value;
};
}  // namespace localization::models

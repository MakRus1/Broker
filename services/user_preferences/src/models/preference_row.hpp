#pragma once
#include <cstdint>
#include <string>
#include <userver/formats/json/value.hpp>

namespace user_preferences::models {
struct PreferenceRow {
    std::string Key;
    userver::formats::json::Value Value;
    std::int64_t Revision;
};
}  // namespace user_preferences::models

#pragma once
#include <cstdint>
#include <optional>
#include <userver/formats/json/value.hpp>

namespace localization::models {
struct EditorRow {
    std::int64_t Version;
    std::int64_t Revision;
    std::optional<userver::formats::json::Value> Draft;
};
}  // namespace localization::models

#pragma once
#include <optional>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/utils/datetime/date.hpp>

namespace sales::models {
struct RegistryRow {
    userver::formats::json::Value Document;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};

}  // namespace sales::models

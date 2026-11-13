#pragma once
#include <optional>
#include <string>
#include <userver/formats/json/value.hpp>

namespace organizations::models {
struct ProvisioningRow {
    std::string Organization;
    std::string Actor;
    userver::formats::json::Value Payload;
    std::optional<std::string> Subject;
};
}  // namespace organizations::models

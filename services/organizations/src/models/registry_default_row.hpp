#pragma once
#include <cstdint>
#include <string>
#include <userver/formats/json/value.hpp>

namespace organizations::models {
struct RegistryDefaultRow {
    std::string Role;
    std::string Registry;
    userver::formats::json::Value Value;
    std::int64_t Revision;
};
}  // namespace organizations::models

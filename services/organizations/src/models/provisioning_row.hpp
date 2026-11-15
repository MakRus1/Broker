#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <userver/formats/json/value.hpp>

namespace organizations::models {
struct ProvisioningRow {
    boost::uuids::uuid Organization;
    std::string Actor;
    userver::formats::json::Value Payload;
    std::optional<std::string> Subject;
};
}  // namespace organizations::models

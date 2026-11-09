#pragma once
#include <openapi/models.hpp>

namespace organizations::converters {
struct RegistryDefaultRow {
    std::string Role;
    std::string Registry;
    userver::formats::json::Value Value;
    std::int64_t Revision;
};

inline models::RegistryDefault Convert(const RegistryDefaultRow& row) {
    return {
        .registry = row.Registry, .revision = row.Revision, .role = row.Role, .value = row.Value};
}

}  // namespace organizations::converters

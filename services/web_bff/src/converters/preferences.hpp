#pragma once
#include <clients/organizations/client.hpp>
#include <clients/user_preferences/client.hpp>
#include <openapi/models.hpp>

namespace web_bff::converters {
inline models::preferences::Preference Convert(
    const clients::user_preferences::models::Preference& value) {
    return {.key = value.key, .revision = value.revision, .value = value.value};
}

inline models::RegistryDefault Convert(
    const clients::organizations::models::RegistryDefault& value) {
    return {.registry = value.registry,
        .revision = value.revision,
        .role = value.role,
        .value = value.value};
}

inline models::RegistryRole Convert(const clients::organizations::models::RegistryRole& value) {
    return {.id = value.id, .name = value.name};
}
}  // namespace web_bff::converters

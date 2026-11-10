#pragma once
#include <clients/transport_locations/client.hpp>
#include <openapi/models.hpp>

namespace transportations::converters {
inline models::RailwayStation Convert(
    const clients::transport_locations::models::RailwayStation& value) {
    return {.country_code = value.country_code,
        .esr_code = value.esr_code,
        .esr_source = value.esr_source,
        .id = value.id,
        .name = value.name,
        .uic_code = value.uic_code};
}
}  // namespace transportations::converters

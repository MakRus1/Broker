#pragma once
#include <clients/transport_locations/client.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline models::shipments::TransportLocation Convert(
    const clients::transport_locations::models::TransportLocation& value) {
    return {.code = value.code,
        .country_code = value.country_code,
        .id = value.id,
        .kind = models::shipments::ParseEnumScalar(
            clients::transport_locations::models::EnumValue(value.kind),
            models::shipments::TransportLocationKind{}),
        .name = value.name};
}

inline models::transportations::RailwayStation Convert(
    const clients::transport_locations::models::RailwayStation& value) {
    return {.country_code = value.country_code,
        .esr_code = value.esr_code,
        .esr_source = value.esr_source,
        .id = value.id,
        .name = value.name,
        .uic_code = value.uic_code};
}
}  // namespace web_bff

#pragma once
#include <clients/transport_locations/client.hpp>
#include <openapi/models.hpp>

namespace admin_bff {
inline models::TransportLocation Convert(
    const clients::transport_locations::models::TransportLocation& value) {
    return {.code = value.code,
        .country_code = value.country_code,
        .id = value.id,
        .kind = value.kind == clients::transport_locations::models::TransportLocationKind::SeaPort
            ? models::TransportLocationKind::SeaPort
            : models::TransportLocationKind::Airport,
        .name = value.name};
}
}  // namespace admin_bff

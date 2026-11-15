#pragma once
#include <clients/transport_locations/client.hpp>
#include <openapi/models.hpp>

namespace sales::converters {
inline models::TransportLocation ToTransportLocation(
    const clients::transport_locations::models::TransportLocation& value) {
    return {.code = value.code,
        .country_code = value.country_code,
        .id = value.id,
        .kind = models::ParseEnumScalar(clients::transport_locations::models::EnumValue(value.kind),
            models::TransportLocationKind{}),
        .name = value.name};
}
}  // namespace sales::converters

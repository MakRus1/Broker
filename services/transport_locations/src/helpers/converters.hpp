#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace transport_locations::helpers {
inline models::TransportLocation Convert(const pgtypes::TransportLocationV1& value) {
    return {.code = value.Code,
        .country_code = value.CountryCode,
        .id = boost::uuids::to_string(value.Id),
        .kind = models::ParseEnumScalar(value.Kind, models::TransportLocationKind{}),
        .name = value.Name};
}

inline models::RailwayStation Convert(const pgtypes::RailwayStationV1& value) {
    return {.country_code = value.CountryCode, .esr_code = value.EsrCode, .name = value.Name};
}
}  // namespace transport_locations::helpers

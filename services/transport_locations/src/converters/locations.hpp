#pragma once
#include <models/pg/railway_station_raw.hpp>
#include <models/pg/transport_location_raw.hpp>
#include <openapi/models.hpp>

namespace transport_locations::converters {
inline models::TransportLocation Convert(const models::pg::TransportLocationRaw& value) {
    return {.code = value.Code,
        .country_code = value.CountryCode,
        .id = boost::uuids::to_string(value.Id),
        .kind = models::ParseEnumScalar(value.Kind, models::TransportLocationKind{}),
        .name = value.Name};
}

inline models::RailwayStation Convert(const models::pg::RailwayStationRaw& value) {
    return {.country_code = value.CountryCode,
        .esr_code = value.EsrCode,
        .esr_source = value.EsrSource,
        .id = boost::uuids::to_string(value.Id),
        .name = value.Name,
        .uic_code = value.UicCode};
}
}  // namespace transport_locations::converters

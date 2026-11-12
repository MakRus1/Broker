#pragma once
#include <clients/transportations/client.hpp>
#include <openapi/models.hpp>

namespace shipments::converters {
inline clients::transportations::models::TransportLocation ToRouteProjection(
    const models::TransportLocation& value) {
    return {.code = value.code,
        .country_code = value.country_code,
        .id = value.id,
        .kind = clients::transportations::models::ParseEnumScalar(models::EnumValue(value.kind),
            clients::transportations::models::TransportLocationKind{}),
        .name = value.name};
}

inline clients::transportations::models::SeaDetails ToRouteProjection(
    const models::SeaDetails& value) {
    clients::transportations::models::SeaDetails result{
        .arrival_terminal_id = value.arrival_terminal_id,
        .departure_date = value.departure_date,
        .departure_terminal_id = value.departure_terminal_id,
        .eta = value.eta,
        .vessel = value.vessel,
        .voyage = value.voyage};
    if (value.pol.has_value())
        result.pol = ToRouteProjection(value.pol.value());
    if (value.pod.has_value())
        result.pod = ToRouteProjection(value.pod.value());
    return result;
}

inline clients::transportations::models::AirDetails ToRouteProjection(
    const models::AirDetails& value) {
    clients::transportations::models::AirDetails result{.arrival_date = value.arrival_date,
        .carrier_id = value.carrier_id,
        .departure_date = value.departure_date,
        .eta = value.eta,
        .etd = value.etd,
        .flight_number = value.flight_number};
    if (value.origin_airport.has_value())
        result.origin_airport = ToRouteProjection(value.origin_airport.value());
    if (value.destination_airport.has_value())
        result.destination_airport = ToRouteProjection(value.destination_airport.value());
    return result;
}

inline clients::transportations::models::ProjectedRouteSegment ToRouteProjection(
    const models::ShipmentRouteSegment& value) {
    clients::transportations::models::ProjectedRouteSegment result{
        .id = value.id, .type = value.type};
    if (value.sea.has_value())
        result.sea = ToRouteProjection(value.sea.value());
    if (value.air.has_value())
        result.air = ToRouteProjection(value.air.value());
    return result;
}
}  // namespace shipments::converters

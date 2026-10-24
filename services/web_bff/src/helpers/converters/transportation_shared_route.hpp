#pragma once
#include <clients/transportations/client.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline models::transportations::TransportLocation Convert(
    const clients::transportations::models::TransportLocation& value) {
    models::transportations::TransportLocation result;
    result.id = value.id;
    result.kind = value.kind == clients::transportations::models::TransportLocationKind::SeaPort
        ? models::transportations::TransportLocationKind::SeaPort
        : models::transportations::TransportLocationKind::Airport;
    result.name = value.name;
    result.country_code = value.country_code;
    if (value.code.has_value())
        result.code = value.code.value();
    return result;
}

inline models::transportations::SeaDetails Convert(
    const clients::transportations::models::SeaDetails& value) {
    models::transportations::SeaDetails result;
    if (value.pol.has_value())
        result.pol = Convert(value.pol.value());
    if (value.pod.has_value())
        result.pod = Convert(value.pod.value());
    if (value.eta.has_value())
        result.eta = value.eta.value();
    if (value.departure_terminal_id.has_value())
        result.departure_terminal_id = value.departure_terminal_id.value();
    if (value.arrival_terminal_id.has_value())
        result.arrival_terminal_id = value.arrival_terminal_id.value();
    if (value.departure_date.has_value())
        result.departure_date = value.departure_date.value();
    if (value.vessel.has_value())
        result.vessel = value.vessel.value();
    if (value.voyage.has_value())
        result.voyage = value.voyage.value();
    return result;
}

inline models::transportations::AirDetails Convert(
    const clients::transportations::models::AirDetails& value) {
    models::transportations::AirDetails result;
    if (value.carrier_id.has_value())
        result.carrier_id = value.carrier_id.value();
    if (value.origin_airport.has_value())
        result.origin_airport = Convert(value.origin_airport.value());
    if (value.destination_airport.has_value())
        result.destination_airport = Convert(value.destination_airport.value());
    if (value.eta.has_value())
        result.eta = value.eta.value();
    if (value.etd.has_value())
        result.etd = value.etd.value();
    if (value.departure_date.has_value())
        result.departure_date = value.departure_date.value();
    if (value.arrival_date.has_value())
        result.arrival_date = value.arrival_date.value();
    if (value.flight_number.has_value())
        result.flight_number = value.flight_number.value();
    return result;
}
}  // namespace web_bff

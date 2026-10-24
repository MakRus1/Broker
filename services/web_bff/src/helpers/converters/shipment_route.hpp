#pragma once
#include <clients/shipments/client.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline models::shipments::TransportLocation Convert(
    const clients::shipments::models::TransportLocation& value) {
    models::shipments::TransportLocation result;
    result.id = value.id;
    result.kind = value.kind == clients::shipments::models::TransportLocationKind::SeaPort
        ? models::shipments::TransportLocationKind::SeaPort
        : models::shipments::TransportLocationKind::Airport;
    result.name = value.name;
    result.country_code = value.country_code;
    if (value.code.has_value())
        result.code = value.code.value();
    return result;
}

inline models::shipments::SeaDetails Convert(const clients::shipments::models::SeaDetails& value) {
    models::shipments::SeaDetails result;
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

inline models::shipments::AirDetails Convert(const clients::shipments::models::AirDetails& value) {
    models::shipments::AirDetails result;
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

inline clients::shipments::models::TransportLocation Convert(
    const models::shipments::TransportLocation& value) {
    clients::shipments::models::TransportLocation result;
    result.id = value.id;
    result.kind = value.kind == models::shipments::TransportLocationKind::SeaPort
        ? clients::shipments::models::TransportLocationKind::SeaPort
        : clients::shipments::models::TransportLocationKind::Airport;
    result.name = value.name;
    result.country_code = value.country_code;
    if (value.code.has_value())
        result.code = value.code.value();
    return result;
}

inline clients::shipments::models::SeaDetails Convert(const models::shipments::SeaDetails& value) {
    clients::shipments::models::SeaDetails result;
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

inline clients::shipments::models::AirDetails Convert(const models::shipments::AirDetails& value) {
    clients::shipments::models::AirDetails result;
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

inline models::shipments::ShipmentRouteSegment Convert(
    const clients::shipments::models::ShipmentRouteSegment& value) {
    models::shipments::ShipmentRouteSegment result{.id = value.id, .type = value.type};
    if (value.sea.has_value())
        result.sea = Convert(value.sea.value());
    if (value.air.has_value())
        result.air = Convert(value.air.value());
    return result;
}

inline clients::shipments::models::ShipmentRouteSegment Convert(
    const models::shipments::ShipmentRouteSegment& value) {
    clients::shipments::models::ShipmentRouteSegment result{.id = value.id, .type = value.type};
    if (value.sea.has_value())
        result.sea = Convert(value.sea.value());
    if (value.air.has_value())
        result.air = Convert(value.air.value());
    return result;
}
}  // namespace web_bff

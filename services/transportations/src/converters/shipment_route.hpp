#pragma once
#include <clients/shipments/client.hpp>
#include <openapi/models.hpp>

namespace transportations::converters {
inline models::TransportLocation Convert(
    const clients::shipments::models::TransportLocation& value) {
    models::TransportLocation result;
    result.id = value.id;
    result.kind = value.kind == clients::shipments::models::TransportLocationKind::SeaPort
        ? models::TransportLocationKind::SeaPort
        : models::TransportLocationKind::Airport;
    result.name = value.name;
    result.country_code = value.country_code;
    if (value.code.has_value())
        result.code = value.code.value();
    return result;
}

inline models::SeaDetails Convert(const clients::shipments::models::SeaDetails& value) {
    models::SeaDetails result;
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
    if (value.booking)
        result.booking = broker_codegen::ParseJson<models::RouteBooking>(
            userver::formats::json::ValueBuilder(*value.booking).ExtractValue());
    if (value.documents)
        result.documents = broker_codegen::ParseJson<std::vector<models::BillOfLading>>(
            userver::formats::json::ValueBuilder(*value.documents).ExtractValue());
    if (value.original_received)
        result.original_received = broker_codegen::ParseJson<models::RouteDocumentEvent>(
            userver::formats::json::ValueBuilder(*value.original_received).ExtractValue());
    if (value.telex_release)
        result.telex_release = broker_codegen::ParseJson<models::RouteDocumentEvent>(
            userver::formats::json::ValueBuilder(*value.telex_release).ExtractValue());
    return result;
}

inline models::AirDetails Convert(const clients::shipments::models::AirDetails& value) {
    models::AirDetails result;
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
    if (value.documents)
        result.documents = broker_codegen::ParseJson<std::vector<models::AirWaybill>>(
            userver::formats::json::ValueBuilder(*value.documents).ExtractValue());
    return result;
}

inline models::RoutePlan Convert(const clients::shipments::models::RoutePlan& value) {
    return {.arrival_date = value.arrival_date,
        .departure_date = value.departure_date,
        .destination_address = value.destination_address,
        .destination_country = value.destination_country,
        .destination_name = value.destination_name,
        .origin_address = value.origin_address,
        .origin_country = value.origin_country,
        .origin_name = value.origin_name};
}

}  // namespace transportations::converters

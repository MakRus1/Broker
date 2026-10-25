#pragma once
#include <boost/uuid/string_generator.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace shipments::helpers {
inline models::TransportLocation Convert(const pgtypes::TransportLocationV1& value) {
    models::TransportLocation result;
    result.id = boost::uuids::to_string(value.Id);
    result.kind = models::ParseEnumScalar(value.Kind, models::TransportLocationKind{});
    result.name = value.Name;
    result.country_code = value.CountryCode;
    if (value.Code.has_value())
        result.code = value.Code.value();
    return result;
}

inline pgtypes::TransportLocationV1 Convert(const models::TransportLocation& value) {
    pgtypes::TransportLocationV1 result;
    result.Id = boost::uuids::string_generator{}(value.id);
    result.Kind = models::EnumValue(value.kind);
    result.Name = value.name;
    result.CountryCode = value.country_code;
    if (value.code.has_value())
        result.Code = value.code.value();
    return result;
}

inline models::SeaDetails Convert(const pgtypes::SeaDetailsV2& value) {
    models::SeaDetails result;
    if (value.Pol.has_value())
        result.pol = Convert(value.Pol.value());
    if (value.Pod.has_value())
        result.pod = Convert(value.Pod.value());
    if (value.Eta.has_value())
        result.eta = value.Eta.value();
    if (value.DepartureTerminalId.has_value())
        result.departure_terminal_id = boost::uuids::to_string(value.DepartureTerminalId.value());
    if (value.ArrivalTerminalId.has_value())
        result.arrival_terminal_id = boost::uuids::to_string(value.ArrivalTerminalId.value());
    if (value.DepartureDate.has_value())
        result.departure_date = value.DepartureDate.value();
    if (value.Vessel.has_value())
        result.vessel = value.Vessel.value();
    if (value.Voyage.has_value())
        result.voyage = value.Voyage.value();
    return result;
}

inline pgtypes::SeaDetailsV2 Convert(const models::SeaDetails& value) {
    pgtypes::SeaDetailsV2 result;
    if (value.pol.has_value())
        result.Pol = Convert(value.pol.value());
    if (value.pod.has_value())
        result.Pod = Convert(value.pod.value());
    if (value.eta.has_value())
        result.Eta = value.eta.value();
    if (value.departure_terminal_id.has_value())
        result.DepartureTerminalId =
            boost::uuids::string_generator{}(value.departure_terminal_id.value());
    if (value.arrival_terminal_id.has_value())
        result.ArrivalTerminalId =
            boost::uuids::string_generator{}(value.arrival_terminal_id.value());
    if (value.departure_date.has_value())
        result.DepartureDate = value.departure_date.value();
    if (value.vessel.has_value())
        result.Vessel = value.vessel.value();
    if (value.voyage.has_value())
        result.Voyage = value.voyage.value();
    return result;
}

inline models::AirDetails Convert(const pgtypes::AirDetailsV2& value) {
    models::AirDetails result;
    if (value.CarrierId.has_value())
        result.carrier_id = boost::uuids::to_string(value.CarrierId.value());
    if (value.OriginAirport.has_value())
        result.origin_airport = Convert(value.OriginAirport.value());
    if (value.DestinationAirport.has_value())
        result.destination_airport = Convert(value.DestinationAirport.value());
    if (value.Eta.has_value())
        result.eta = value.Eta.value();
    if (value.Etd.has_value())
        result.etd = value.Etd.value();
    if (value.DepartureDate.has_value())
        result.departure_date = value.DepartureDate.value();
    if (value.ArrivalDate.has_value())
        result.arrival_date = value.ArrivalDate.value();
    if (value.FlightNumber.has_value())
        result.flight_number = value.FlightNumber.value();
    return result;
}

inline pgtypes::AirDetailsV2 Convert(const models::AirDetails& value) {
    pgtypes::AirDetailsV2 result;
    if (value.carrier_id.has_value())
        result.CarrierId = boost::uuids::string_generator{}(value.carrier_id.value());
    if (value.origin_airport.has_value())
        result.OriginAirport = Convert(value.origin_airport.value());
    if (value.destination_airport.has_value())
        result.DestinationAirport = Convert(value.destination_airport.value());
    if (value.eta.has_value())
        result.Eta = value.eta.value();
    if (value.etd.has_value())
        result.Etd = value.etd.value();
    if (value.departure_date.has_value())
        result.DepartureDate = value.departure_date.value();
    if (value.arrival_date.has_value())
        result.ArrivalDate = value.arrival_date.value();
    if (value.flight_number.has_value())
        result.FlightNumber = value.flight_number.value();
    return result;
}

inline models::ShipmentRouteSegment Convert(const pgtypes::ShipmentRouteSegmentV3& value) {
    models::ShipmentRouteSegment result;
    result.id = boost::uuids::to_string(value.Id);
    result.type = value.Type;
    if (value.Sea.has_value())
        result.sea = Convert(value.Sea.value());
    if (value.Air.has_value())
        result.air = Convert(value.Air.value());
    return result;
}

inline pgtypes::ShipmentRouteSegmentV3 Convert(const models::ShipmentRouteSegment& value) {
    pgtypes::ShipmentRouteSegmentV3 result;
    result.Id = boost::uuids::string_generator{}(value.id);
    result.Type = value.type;
    if (value.sea.has_value())
        result.Sea = Convert(value.sea.value());
    if (value.air.has_value())
        result.Air = Convert(value.air.value());
    return result;
}
}  // namespace shipments::helpers

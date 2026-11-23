#pragma once
#include <models/pg/air_details.hpp>
#include <models/pg/sea_details.hpp>
#include <models/pg/shipment_route_segment.hpp>
#include <models/pg/transport_location.hpp>
#include <openapi/models.hpp>

namespace shipments::converters {
inline models::TransportLocation Convert(const models::pg::TransportLocation& value) {
    models::TransportLocation result;
    result.id = value.Id;
    result.kind = models::ParseEnumScalar(value.Kind, models::TransportLocationKind{});
    result.name = value.Name;
    result.country_code = value.CountryCode;
    if (value.Code.has_value())
        result.code = value.Code.value();
    return result;
}

inline models::pg::TransportLocation Convert(const models::TransportLocation& value) {
    models::pg::TransportLocation result;
    result.Id = value.id;
    result.Kind = models::EnumValue(value.kind);
    result.Name = value.name;
    result.CountryCode = value.country_code;
    if (value.code.has_value())
        result.Code = value.code.value();
    return result;
}

inline models::RouteBooking Convert(const models::pg::RouteBooking& value) {
    models::RouteBooking result;
    result.number = value.Number;
    result.placed_date = value.PlacedDate;
    result.confirmed_date = value.ConfirmedDate;
    if (value.Status.has_value())
        result.status =
            models::ParseEnumScalar(value.Status.value(), models::RouteBooking_status{});
    return result;
}

inline models::pg::RouteBooking Convert(const models::RouteBooking& value) {
    models::pg::RouteBooking result;
    result.Number = value.number;
    result.PlacedDate = value.placed_date;
    result.ConfirmedDate = value.confirmed_date;
    if (value.status.has_value())
        result.Status = models::EnumValue(value.status.value());
    return result;
}

inline models::BillOfLading Convert(const models::pg::BillOfLading& value) {
    models::BillOfLading result;
    result.number = value.Number;
    result.date = value.Date;
    if (value.Leg.has_value())
        result.leg = models::ParseEnumScalar(value.Leg.value(), models::BillOfLading_leg{});
    return result;
}

inline models::pg::BillOfLading Convert(const models::BillOfLading& value) {
    models::pg::BillOfLading result;
    result.Number = value.number;
    result.Date = value.date;
    if (value.leg.has_value())
        result.Leg = models::EnumValue(value.leg.value());
    return result;
}

inline models::AirWaybill Convert(const models::pg::AirWaybill& value) {
    return {.date = value.Date, .number = value.Number};
}

inline models::pg::AirWaybill Convert(const models::AirWaybill& value) {
    return {.Number = value.number, .Date = value.date};
}

inline models::RouteDocumentEvent Convert(const models::pg::RouteDocumentEvent& value) {
    return {.date = value.Date};
}

inline models::pg::RouteDocumentEvent Convert(const models::RouteDocumentEvent& value) {
    return {.Date = value.date};
}

template <typename Target, typename Source>
std::optional<std::vector<Target>> ConvertDocuments(
    const std::optional<std::vector<Source>>& documents) {
    if (!documents.has_value())
        return std::nullopt;
    std::vector<Target> result;
    result.reserve(documents.value().size());
    for (const auto& document : documents.value())
        result.push_back(Convert(document));
    return result;
}

inline models::SeaDetails Convert(const models::pg::SeaDetails& value) {
    models::SeaDetails result;
    if (value.Pol.has_value())
        result.pol = Convert(value.Pol.value());
    if (value.Pod.has_value())
        result.pod = Convert(value.Pod.value());
    if (value.Eta.has_value())
        result.eta = value.Eta.value();
    if (value.DepartureTerminalId.has_value())
        result.departure_terminal_id = value.DepartureTerminalId.value();
    if (value.ArrivalTerminalId.has_value())
        result.arrival_terminal_id = value.ArrivalTerminalId.value();
    if (value.DepartureDate.has_value())
        result.departure_date = value.DepartureDate.value();
    if (value.Vessel.has_value())
        result.vessel = value.Vessel.value();
    if (value.Voyage.has_value())
        result.voyage = value.Voyage.value();
    if (value.Booking.has_value())
        result.booking = Convert(value.Booking.value());
    result.documents = ConvertDocuments<models::BillOfLading>(value.Documents);
    if (value.OriginalReceived.has_value())
        result.original_received = Convert(value.OriginalReceived.value());
    if (value.TelexRelease.has_value())
        result.telex_release = Convert(value.TelexRelease.value());
    return result;
}

inline models::pg::SeaDetails Convert(const models::SeaDetails& value) {
    models::pg::SeaDetails result;
    if (value.pol.has_value())
        result.Pol = Convert(value.pol.value());
    if (value.pod.has_value())
        result.Pod = Convert(value.pod.value());
    if (value.eta.has_value())
        result.Eta = value.eta.value();
    if (value.departure_terminal_id.has_value())
        result.DepartureTerminalId = value.departure_terminal_id.value();
    if (value.arrival_terminal_id.has_value())
        result.ArrivalTerminalId = value.arrival_terminal_id.value();
    if (value.departure_date.has_value())
        result.DepartureDate = value.departure_date.value();
    if (value.vessel.has_value())
        result.Vessel = value.vessel.value();
    if (value.voyage.has_value())
        result.Voyage = value.voyage.value();
    if (value.booking.has_value())
        result.Booking = Convert(value.booking.value());
    result.Documents = ConvertDocuments<models::pg::BillOfLading>(value.documents);
    if (value.original_received.has_value())
        result.OriginalReceived = Convert(value.original_received.value());
    if (value.telex_release.has_value())
        result.TelexRelease = Convert(value.telex_release.value());
    return result;
}

inline models::AirDetails Convert(const models::pg::AirDetails& value) {
    models::AirDetails result;
    if (value.CarrierId.has_value())
        result.carrier_id = value.CarrierId.value();
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
    result.documents = ConvertDocuments<models::AirWaybill>(value.Documents);
    return result;
}

inline models::pg::AirDetails Convert(const models::AirDetails& value) {
    models::pg::AirDetails result;
    if (value.carrier_id.has_value())
        result.CarrierId = value.carrier_id.value();
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
    result.Documents = ConvertDocuments<models::pg::AirWaybill>(value.documents);
    return result;
}

inline models::RoutePlan Convert(const models::pg::RoutePlan& value) {
    models::RoutePlan result;
    result.origin_name = value.OriginName;
    result.origin_country = value.OriginCountry;
    result.origin_address = value.OriginAddress;
    result.destination_name = value.DestinationName;
    result.destination_country = value.DestinationCountry;
    result.destination_address = value.DestinationAddress;
    result.departure_date = value.DepartureDate;
    result.arrival_date = value.ArrivalDate;
    return result;
}

inline models::pg::RoutePlan Convert(const models::RoutePlan& value) {
    models::pg::RoutePlan result;
    result.OriginName = value.origin_name;
    result.OriginCountry = value.origin_country;
    result.OriginAddress = value.origin_address;
    result.DestinationName = value.destination_name;
    result.DestinationCountry = value.destination_country;
    result.DestinationAddress = value.destination_address;
    result.DepartureDate = value.departure_date;
    result.ArrivalDate = value.arrival_date;
    return result;
}

inline models::ShipmentRouteSegment Convert(const models::pg::ShipmentRouteSegment& value) {
    models::ShipmentRouteSegment result;
    result.id = value.Id;
    result.type = value.Type;
    if (value.Plan.has_value())
        result.plan = Convert(value.Plan.value());
    if (value.Sea.has_value())
        result.sea = Convert(value.Sea.value());
    if (value.Air.has_value())
        result.air = Convert(value.Air.value());
    return result;
}

inline models::pg::ShipmentRouteSegment Convert(const models::ShipmentRouteSegment& value) {
    models::pg::ShipmentRouteSegment result;
    result.Id = value.id;
    result.Type = value.type;
    if (value.plan.has_value())
        result.Plan = Convert(value.plan.value());
    if (value.sea.has_value())
        result.Sea = Convert(value.sea.value());
    if (value.air.has_value())
        result.Air = Convert(value.air.value());
    return result;
}
}  // namespace shipments::converters

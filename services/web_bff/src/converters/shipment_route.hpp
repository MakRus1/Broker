#pragma once
#include <clients/shipments/client.hpp>
#include <openapi/models.hpp>

namespace web_bff::converters {
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
    if (value.booking.has_value()) {
        const auto& source = value.booking.value();
        auto& booking = result.booking.emplace();
        booking.number = source.number;
        booking.placed_date = source.placed_date;
        booking.confirmed_date = source.confirmed_date;
        if (source.status.has_value())
            booking.status = models::shipments::ParseEnumScalar(
                EnumValue(source.status.value()), models::shipments::RouteBooking_status{});
    }
    if (value.documents.has_value()) {
        auto& documents = result.documents.emplace();
        documents.reserve(value.documents.value().size());
        for (const auto& source : value.documents.value()) {
            auto& document = documents.emplace_back();
            document.number = source.number;
            document.date = source.date;
            if (source.leg.has_value())
                document.leg = models::shipments::ParseEnumScalar(
                    EnumValue(source.leg.value()), models::shipments::BillOfLading_leg{});
        }
    }
    if (value.original_received.has_value())
        result.original_received.emplace().date = value.original_received.value().date;
    if (value.telex_release.has_value())
        result.telex_release.emplace().date = value.telex_release.value().date;
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
    if (value.documents.has_value()) {
        auto& documents = result.documents.emplace();
        documents.reserve(value.documents.value().size());
        for (const auto& source : value.documents.value()) {
            auto& document = documents.emplace_back();
            document.number = source.number;
            document.date = source.date;
        }
    }
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
    if (value.booking.has_value()) {
        const auto& source = value.booking.value();
        auto& booking = result.booking.emplace();
        booking.number = source.number;
        booking.placed_date = source.placed_date;
        booking.confirmed_date = source.confirmed_date;
        if (source.status.has_value())
            booking.status =
                clients::shipments::models::ParseEnumScalar(EnumValue(source.status.value()),
                    clients::shipments::models::RouteBooking_status{});
    }
    if (value.documents.has_value()) {
        auto& documents = result.documents.emplace();
        documents.reserve(value.documents.value().size());
        for (const auto& source : value.documents.value()) {
            auto& document = documents.emplace_back();
            document.number = source.number;
            document.date = source.date;
            if (source.leg.has_value())
                document.leg = clients::shipments::models::ParseEnumScalar(
                    EnumValue(source.leg.value()), clients::shipments::models::BillOfLading_leg{});
        }
    }
    if (value.original_received.has_value())
        result.original_received.emplace().date = value.original_received.value().date;
    if (value.telex_release.has_value())
        result.telex_release.emplace().date = value.telex_release.value().date;
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
    if (value.documents.has_value()) {
        auto& documents = result.documents.emplace();
        documents.reserve(value.documents.value().size());
        for (const auto& source : value.documents.value()) {
            auto& document = documents.emplace_back();
            document.number = source.number;
            document.date = source.date;
        }
    }
    return result;
}

inline models::shipments::RoutePlan Convert(const clients::shipments::models::RoutePlan& value) {
    return {.arrival_date = value.arrival_date,
        .departure_date = value.departure_date,
        .destination_address = value.destination_address,
        .destination_country = value.destination_country,
        .destination_name = value.destination_name,
        .origin_address = value.origin_address,
        .origin_country = value.origin_country,
        .origin_name = value.origin_name};
}

inline models::shipments::ShipmentRouteSegment Convert(
    const clients::shipments::models::ShipmentRouteSegment& value) {
    models::shipments::ShipmentRouteSegment result{.id = value.id, .type = value.type};
    if (value.plan)
        result.plan = Convert(*value.plan);
    if (value.sea.has_value())
        result.sea = Convert(value.sea.value());
    if (value.air.has_value())
        result.air = Convert(value.air.value());
    return result;
}

inline clients::shipments::models::RoutePlan Convert(const models::shipments::RoutePlan& value) {
    return {.arrival_date = value.arrival_date,
        .departure_date = value.departure_date,
        .destination_address = value.destination_address,
        .destination_country = value.destination_country,
        .destination_name = value.destination_name,
        .origin_address = value.origin_address,
        .origin_country = value.origin_country,
        .origin_name = value.origin_name};
}

inline clients::shipments::models::ShipmentRouteSegment Convert(
    const models::shipments::ShipmentRouteSegment& value) {
    clients::shipments::models::ShipmentRouteSegment result{.id = value.id, .type = value.type};
    if (value.plan)
        result.plan = Convert(*value.plan);
    if (value.sea.has_value())
        result.sea = Convert(value.sea.value());
    if (value.air.has_value())
        result.air = Convert(value.air.value());
    return result;
}
}  // namespace web_bff::converters

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
    if (value.booking.has_value()) {
        const auto& source = value.booking.value();
        auto& booking = result.booking.emplace();
        booking.number = source.number;
        booking.placed_date = source.placed_date;
        booking.confirmed_date = source.confirmed_date;
        if (source.status.has_value())
            booking.status =
                clients::transportations::models::ParseEnumScalar(EnumValue(source.status.value()),
                    clients::transportations::models::RouteBooking_status{});
    }
    if (value.documents.has_value()) {
        auto& documents = result.documents.emplace();
        documents.reserve(value.documents.value().size());
        for (const auto& source : value.documents.value()) {
            auto& document = documents.emplace_back();
            document.number = source.number;
            document.date = source.date;
            if (source.leg.has_value())
                document.leg =
                    clients::transportations::models::ParseEnumScalar(EnumValue(source.leg.value()),
                        clients::transportations::models::BillOfLading_leg{});
        }
    }
    if (value.original_received.has_value())
        result.original_received.emplace().date = value.original_received.value().date;
    if (value.telex_release.has_value())
        result.telex_release.emplace().date = value.telex_release.value().date;
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

inline clients::transportations::models::RoutePlan ToRouteProjection(
    const models::RoutePlan& value) {
    return {.arrival_date = value.arrival_date,
        .departure_date = value.departure_date,
        .destination_address = value.destination_address,
        .destination_country = value.destination_country,
        .destination_name = value.destination_name,
        .origin_address = value.origin_address,
        .origin_country = value.origin_country,
        .origin_name = value.origin_name};
}

inline clients::transportations::models::ProjectedRouteSegment ToRouteProjection(
    const models::ShipmentRouteSegment& value) {
    clients::transportations::models::ProjectedRouteSegment result{
        .id = value.id, .type = value.type};
    if (value.plan.has_value())
        result.plan = ToRouteProjection(value.plan.value());
    if (value.sea.has_value())
        result.sea = ToRouteProjection(value.sea.value());
    if (value.air.has_value())
        result.air = ToRouteProjection(value.air.value());
    return result;
}
}  // namespace shipments::converters

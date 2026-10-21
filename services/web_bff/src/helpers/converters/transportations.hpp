#pragma once
#include <clients/transportations/client.hpp>
#include <openapi/models.hpp>
#include <stdexcept>

namespace web_bff {
inline models::transportations::ContainerOwnership Convert(
    clients::transportations::models::ContainerOwnership value) {
    switch (value) {
    case clients::transportations::models::ContainerOwnership::SOC:
        return models::transportations::ContainerOwnership::SOC;
    case clients::transportations::models::ContainerOwnership::COC:
        return models::transportations::ContainerOwnership::COC;
    }
    throw std::invalid_argument("Неизвестное значение перечисления клиента");
}

inline models::transportations::TransportMode Convert(
    clients::transportations::models::TransportMode value) {
    switch (value) {
    case clients::transportations::models::TransportMode::Road:
        return models::transportations::TransportMode::Road;
    case clients::transportations::models::TransportMode::Rail:
        return models::transportations::TransportMode::Rail;
    case clients::transportations::models::TransportMode::Sea:
        return models::transportations::TransportMode::Sea;
    }
    throw std::invalid_argument("Неизвестное значение перечисления клиента");
}

inline models::transportations::RouteSegmentOrigin Convert(
    const clients::transportations::models::RouteSegmentOrigin& value) {
    models::transportations::RouteSegmentOrigin result;
    result.country_code = value.country_code;
    result.name = value.name;
    return result;
}

inline models::transportations::RouteSegmentDestination Convert(
    const clients::transportations::models::RouteSegmentDestination& value) {
    models::transportations::RouteSegmentDestination result;
    result.country_code = value.country_code;
    result.name = value.name;
    return result;
}

inline models::transportations::RouteSegment Convert(
    const clients::transportations::models::RouteSegment& value) {
    models::transportations::RouteSegment result;
    result.actual_arrival_at = value.actual_arrival_at;
    result.actual_departure_at = value.actual_departure_at;
    result.contractor_id = value.contractor_id;
    result.destination = Convert(value.destination);
    result.id = value.id;
    result.mode = Convert(value.mode);
    result.origin = Convert(value.origin);
    result.planned_arrival_at = value.planned_arrival_at;
    result.planned_departure_at = value.planned_departure_at;
    return result;
}

inline models::transportations::Transportation Convert(
    const clients::transportations::models::Transportation& value) {
    models::transportations::Transportation result;
    result.container_ownership = Convert(value.container_ownership);
    result.container_type_code = value.container_type_code;
    result.id = value.id;
    result.item_id = value.item_id;
    result.route_segments.reserve(value.route_segments.size());
    for (const auto& item : value.route_segments)
        result.route_segments.push_back(Convert(item));
    result.number = value.number;
    result.operation_id = value.operation_id;
    result.revision = value.revision;
    result.shipment_id = value.shipment_id;
    return result;
}

inline clients::transportations::ApiV1TransportationsGet::RequestBody Convert(
    const models::transportations::GetTransportationRequest& value) {
    return {.transportation_id = value.transportation_id};
}

inline clients::transportations::ApiV1TransportationsList::RequestBody Convert(
    const models::transportations::ListTransportationsRequest& value) {
    return {.after = value.after,
        .item_id = value.item_id,
        .limit = value.limit,
        .shipment_id = value.shipment_id};
}

inline clients::transportations::models::TransportMode Convert(
    models::transportations::TransportMode value) {
    using Target = clients::transportations::models::TransportMode;
    switch (value) {
    case models::transportations::TransportMode::Road:
        return Target::Road;
    case models::transportations::TransportMode::Rail:
        return Target::Rail;
    case models::transportations::TransportMode::Sea:
        return Target::Sea;
    }
    throw std::invalid_argument("Неизвестный вид транспорта");
}

inline clients::transportations::models::EventKind Convert(
    models::transportations::EventKind value) {
    using Target = clients::transportations::models::EventKind;
    switch (value) {
    case models::transportations::EventKind::Departure:
        return Target::Departure;
    case models::transportations::EventKind::Arrival:
        return Target::Arrival;
    }
    throw std::invalid_argument("Неизвестный вид события");
}

inline models::transportations::EventKind Convert(
    clients::transportations::models::EventKind value) {
    switch (value) {
    case clients::transportations::models::EventKind::Departure:
        return models::transportations::EventKind::Departure;
    case clients::transportations::models::EventKind::Arrival:
        return models::transportations::EventKind::Arrival;
    }
    throw std::invalid_argument("Неизвестный вид события");
}

inline clients::transportations::models::SetRouteSegmentsItem Convert(
    const models::transportations::SetRouteSegmentsItem& value) {
    return {.contractor_id = value.contractor_id,
        .destination = {.country_code = value.destination.country_code,
            .name = value.destination.name},
        .id = value.id,
        .mode = Convert(value.mode),
        .origin = {.country_code = value.origin.country_code, .name = value.origin.name},
        .planned_arrival_at = value.planned_arrival_at,
        .planned_departure_at = value.planned_departure_at};
}

inline clients::transportations::ApiV1TransportationRoutesSet::RequestBody Convert(
    const models::transportations::SetRouteRequest& value) {
    clients::transportations::ApiV1TransportationRoutesSet::RequestBody result;
    result.expected_revision = value.expected_revision;
    result.transportation_id = value.transportation_id;
    result.route_segments.reserve(value.route_segments.size());
    for (const auto& route_segment : value.route_segments)
        result.route_segments.push_back(Convert(route_segment));
    return result;
}

inline clients::transportations::ApiV1TransportationActualsRecord::RequestBody Convert(
    const models::transportations::RecordActualRequest& value) {
    return {.event_kind = Convert(value.event_kind),
        .expected_revision = value.expected_revision,
        .occurred_at = value.occurred_at,
        .route_segment_id = value.route_segment_id,
        .transportation_id = value.transportation_id};
}

inline clients::transportations::ApiV1TransportationActualsCorrect::RequestBody Convert(
    const models::transportations::CorrectActualRequest& value) {
    return {.event_kind = Convert(value.event_kind),
        .expected_revision = value.expected_revision,
        .occurred_at = value.occurred_at,
        .reason = value.reason,
        .route_segment_id = value.route_segment_id,
        .transportation_id = value.transportation_id};
}

inline clients::transportations::ApiV1TransportationsHistory::RequestBody Convert(
    const models::transportations::ListHistoryRequest& value) {
    return {
        .before = value.before, .limit = value.limit, .transportation_id = value.transportation_id};
}

inline models::transportations::TransportationEvent Convert(
    const clients::transportations::models::TransportationEvent& value) {
    models::transportations::TransportationEvent result;
    result.action = value.action;
    result.actor = value.actor;
    result.actor_name = value.actor_name;
    result.id = value.id;
    result.occurred_at = value.occurred_at;
    result.reason = value.reason;
    result.route_segment_id = value.route_segment_id;
    if (value.event_kind.has_value())
        result.event_kind = Convert(value.event_kind.value());
    result.revision = value.revision;
    result.route_segments.reserve(value.route_segments.size());
    for (const auto& route_segment : value.route_segments)
        result.route_segments.push_back(Convert(route_segment));
    return result;
}
}  // namespace web_bff

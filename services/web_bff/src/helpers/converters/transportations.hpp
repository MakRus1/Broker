#pragma once
#include <clients/transportations/client.hpp>
#include <openapi/models.hpp>
#include <stdexcept>

namespace web_bff {
inline models::TContainerOwnership Convert(
    clients::transportations::models::ContainerOwnership value) {
    switch (value) {
    case clients::transportations::models::ContainerOwnership::SOC:
        return models::TContainerOwnership::SOC;
    case clients::transportations::models::ContainerOwnership::COC:
        return models::TContainerOwnership::COC;
    }
    throw std::invalid_argument("Неизвестное значение перечисления клиента");
}

inline models::TTransportMode Convert(clients::transportations::models::TransportMode value) {
    switch (value) {
    case clients::transportations::models::TransportMode::Road:
        return models::TTransportMode::Road;
    case clients::transportations::models::TransportMode::Rail:
        return models::TTransportMode::Rail;
    case clients::transportations::models::TransportMode::Sea:
        return models::TTransportMode::Sea;
    }
    throw std::invalid_argument("Неизвестное значение перечисления клиента");
}

inline models::TTransportationLegOrigin Convert(
    const clients::transportations::models::TransportationLegOrigin& value) {
    models::TTransportationLegOrigin result;
    result.country_code = value.country_code;
    result.name = value.name;
    return result;
}

inline models::TTransportationLegDestination Convert(
    const clients::transportations::models::TransportationLegDestination& value) {
    models::TTransportationLegDestination result;
    result.country_code = value.country_code;
    result.name = value.name;
    return result;
}

inline models::TTransportationLeg Convert(
    const clients::transportations::models::TransportationLeg& value) {
    models::TTransportationLeg result;
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

inline models::TTransportation Convert(
    const clients::transportations::models::Transportation& value) {
    models::TTransportation result;
    result.container_ownership = Convert(value.container_ownership);
    result.container_type_code = value.container_type_code;
    result.id = value.id;
    result.item_id = value.item_id;
    result.legs.reserve(value.legs.size());
    for (const auto& item : value.legs)
        result.legs.push_back(Convert(item));
    result.number = value.number;
    result.operation_id = value.operation_id;
    result.revision = value.revision;
    result.shipment_id = value.shipment_id;
    return result;
}

inline clients::transportations::ApiV1TransportationsGet::RequestBody Convert(
    const models::TGetTransportationRequest& value) {
    return {.transportation_id = value.transportation_id};
}

inline clients::transportations::ApiV1TransportationsList::RequestBody Convert(
    const models::TListTransportationsRequest& value) {
    return {.after = value.after,
        .item_id = value.item_id,
        .limit = value.limit,
        .shipment_id = value.shipment_id};
}

inline clients::transportations::models::TransportMode Convert(models::TTransportMode value) {
    using Target = clients::transportations::models::TransportMode;
    switch (value) {
    case models::TTransportMode::Road:
        return Target::Road;
    case models::TTransportMode::Rail:
        return Target::Rail;
    case models::TTransportMode::Sea:
        return Target::Sea;
    }
    throw std::invalid_argument("Неизвестный вид транспорта");
}

inline clients::transportations::models::EventKind Convert(models::TEventKind value) {
    using Target = clients::transportations::models::EventKind;
    switch (value) {
    case models::TEventKind::Departure:
        return Target::Departure;
    case models::TEventKind::Arrival:
        return Target::Arrival;
    }
    throw std::invalid_argument("Неизвестный вид события");
}

inline models::TEventKind Convert(clients::transportations::models::EventKind value) {
    switch (value) {
    case clients::transportations::models::EventKind::Departure:
        return models::TEventKind::Departure;
    case clients::transportations::models::EventKind::Arrival:
        return models::TEventKind::Arrival;
    }
    throw std::invalid_argument("Неизвестный вид события");
}

inline clients::transportations::models::SetRouteLegsItem Convert(
    const models::TSetRouteLegsItem& value) {
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
    const models::TSetRouteRequest& value) {
    clients::transportations::ApiV1TransportationRoutesSet::RequestBody result;
    result.expected_revision = value.expected_revision;
    result.transportation_id = value.transportation_id;
    result.legs.reserve(value.legs.size());
    for (const auto& leg : value.legs)
        result.legs.push_back(Convert(leg));
    return result;
}

inline clients::transportations::ApiV1TransportationActualsRecord::RequestBody Convert(
    const models::TRecordActualRequest& value) {
    return {.event_kind = Convert(value.event_kind),
        .expected_revision = value.expected_revision,
        .leg_id = value.leg_id,
        .occurred_at = value.occurred_at,
        .transportation_id = value.transportation_id};
}

inline clients::transportations::ApiV1TransportationActualsCorrect::RequestBody Convert(
    const models::TCorrectActualRequest& value) {
    return {.event_kind = Convert(value.event_kind),
        .expected_revision = value.expected_revision,
        .leg_id = value.leg_id,
        .occurred_at = value.occurred_at,
        .reason = value.reason,
        .transportation_id = value.transportation_id};
}

inline clients::transportations::ApiV1TransportationsHistory::RequestBody Convert(
    const models::TListHistoryRequest& value) {
    return {
        .before = value.before, .limit = value.limit, .transportation_id = value.transportation_id};
}

inline models::TTransportationEvent Convert(
    const clients::transportations::models::TransportationEvent& value) {
    models::TTransportationEvent result;
    result.action = value.action;
    result.actor = value.actor;
    result.actor_name = value.actor_name;
    result.id = value.id;
    result.occurred_at = value.occurred_at;
    result.reason = value.reason;
    result.leg_id = value.leg_id;
    if (value.event_kind.has_value())
        result.event_kind = Convert(value.event_kind.value());
    result.revision = value.revision;
    result.legs.reserve(value.legs.size());
    for (const auto& leg : value.legs)
        result.legs.push_back(Convert(leg));
    return result;
}
}  // namespace web_bff

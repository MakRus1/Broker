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
}  // namespace web_bff

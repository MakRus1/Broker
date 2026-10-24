#pragma once
#include <clients/shipments/client.hpp>
#include <helpers/converters/shipment_route.hpp>
#include <openapi/models.hpp>
#include <stdexcept>

namespace web_bff {
inline models::shipments::Shipment Convert(const clients::shipments::models::Shipment& value) {
    models::shipments::Shipment result;
    result.customer_id = value.customer_id;
    result.id = value.id;
    result.number = value.number;
    result.reference = value.reference;
    result.shipment_date = value.shipment_date;
    result.recipient_id = value.recipient_id;
    result.logist = value.logist;
    result.cargo_description = value.cargo_description;
    result.status = value.status;
    result.incoterms = value.incoterms;
    result.carriage_terms = value.carriage_terms;
    result.loading = value.loading;
    result.delivery_city = value.delivery_city;
    result.delivery_country = value.delivery_country;
    result.delivery_address = value.delivery_address;
    for (const auto& segment : value.route_segments)
        result.route_segments.push_back(Convert(segment));
    result.revision = value.revision;
    return result;
}

inline models::shipments::ContainerOwnership Convert(
    clients::shipments::models::ContainerOwnership value) {
    switch (value) {
    case clients::shipments::models::ContainerOwnership::SOC:
        return models::shipments::ContainerOwnership::SOC;
    case clients::shipments::models::ContainerOwnership::COC:
        return models::shipments::ContainerOwnership::COC;
    }
    throw std::invalid_argument("Неизвестное значение перечисления клиента");
}

inline models::shipments::ShipmentItem Convert(
    const clients::shipments::models::ShipmentItem& value) {
    models::shipments::ShipmentItem result;
    result.available_quantity = value.available_quantity;
    result.container_ownership = Convert(value.container_ownership);
    result.container_type_code = value.container_type_code;
    result.created_quantity = value.created_quantity;
    result.id = value.id;
    result.pending_quantity = value.pending_quantity;
    result.planned_quantity = value.planned_quantity;
    result.revision = value.revision;
    result.shipment_id = value.shipment_id;
    return result;
}

inline models::shipments::OperationStatus Convert(
    clients::shipments::models::OperationStatus value) {
    switch (value) {
    case clients::shipments::models::OperationStatus::Pending:
        return models::shipments::OperationStatus::Pending;
    case clients::shipments::models::OperationStatus::Succeeded:
        return models::shipments::OperationStatus::Succeeded;
    case clients::shipments::models::OperationStatus::Rejected:
        return models::shipments::OperationStatus::Rejected;
    }
    throw std::invalid_argument("Неизвестное значение перечисления клиента");
}

inline models::shipments::CreationOperation Convert(
    const clients::shipments::models::CreationOperation& value) {
    models::shipments::CreationOperation result;
    result.id = value.id;
    result.item_id = value.item_id;
    result.quantity = value.quantity;
    result.rejection_code = value.rejection_code;
    result.status = Convert(value.status);
    result.transportation_ids = value.transportation_ids;
    return result;
}

inline clients::shipments::models::ContainerOwnership Convert(
    models::shipments::ContainerOwnership value) {
    switch (value) {
    case models::shipments::ContainerOwnership::SOC:
        return clients::shipments::models::ContainerOwnership::SOC;
    case models::shipments::ContainerOwnership::COC:
        return clients::shipments::models::ContainerOwnership::COC;
    }
    throw std::invalid_argument("Неизвестная принадлежность контейнера");
}

inline clients::shipments::ApiV1TransportationOperationsCreate::RequestBody Convert(
    const models::shipments::CreateOperationRequest& value) {
    return {.expected_revision = value.expected_revision,
        .idempotency_key = value.idempotency_key,
        .item_id = value.item_id,
        .quantity = value.quantity};
}

inline clients::shipments::ApiV1TransportationOperationsGet::RequestBody Convert(
    const models::shipments::GetOperationRequest& value) {
    return {.operation_id = value.operation_id};
}

inline std::optional<std::vector<clients::shipments::models::ShipmentRouteSegment>> ConvertSegments(
    const std::optional<std::vector<models::shipments::ShipmentRouteSegment>>& values) {
    if (!values.has_value())
        return std::nullopt;
    std::vector<clients::shipments::models::ShipmentRouteSegment> result;
    for (const auto& value : values.value())
        result.push_back(Convert(value));
    return result;
}

inline clients::shipments::ApiV1ShipmentsCreate::RequestBody Convert(
    const models::shipments::CreateShipmentRequest& value) {
    return {.cargo_description = value.cargo_description,
        .carriage_terms = value.carriage_terms,
        .customer_id = value.customer_id,
        .delivery_address = value.delivery_address,
        .delivery_city = value.delivery_city,
        .delivery_country = value.delivery_country,
        .idempotency_key = value.idempotency_key,
        .incoterms = value.incoterms,
        .loading = value.loading,
        .logist = value.logist,
        .number = value.number,
        .recipient_id = value.recipient_id,
        .reference = value.reference,
        .route_segments = ConvertSegments(value.route_segments),
        .shipment_date = value.shipment_date,
        .status = value.status};
}

inline clients::shipments::ApiV1ShipmentsGet::RequestBody Convert(
    const models::shipments::GetShipmentRequest& value) {
    return {.shipment_id = value.shipment_id};
}

inline clients::shipments::ApiV1ShipmentsList::RequestBody Convert(
    const models::shipments::ListShipmentsRequest& value) {
    return {.after = value.after, .limit = value.limit};
}

inline clients::shipments::ApiV1ShipmentsUpdate::RequestBody Convert(
    const models::shipments::UpdateShipmentRequest& value) {
    return {.cargo_description = value.cargo_description,
        .carriage_terms = value.carriage_terms,
        .customer_id = value.customer_id,
        .delivery_address = value.delivery_address,
        .delivery_city = value.delivery_city,
        .delivery_country = value.delivery_country,
        .expected_revision = value.expected_revision,
        .incoterms = value.incoterms,
        .loading = value.loading,
        .logist = value.logist,
        .number = value.number,
        .recipient_id = value.recipient_id,
        .reference = value.reference,
        .route_segments = ConvertSegments(value.route_segments),
        .shipment_date = value.shipment_date,
        .shipment_id = value.shipment_id,
        .status = value.status};
}

inline clients::shipments::ApiV1ShipmentItemsAdd::RequestBody Convert(
    const models::shipments::AddItemRequest& value) {
    return {.container_ownership = Convert(value.container_ownership),
        .container_type_code = value.container_type_code,
        .expected_revision = value.expected_revision,
        .idempotency_key = value.idempotency_key,
        .planned_quantity = value.planned_quantity,
        .shipment_id = value.shipment_id};
}

inline clients::shipments::ApiV1ShipmentItemsList::RequestBody Convert(
    const models::shipments::ListItemsRequest& value) {
    return {.after = value.after, .limit = value.limit, .shipment_id = value.shipment_id};
}

inline clients::shipments::ApiV1ShipmentItemsUpdate::RequestBody Convert(
    const models::shipments::UpdateItemRequest& value) {
    return {.expected_revision = value.expected_revision,
        .item_id = value.item_id,
        .planned_quantity = value.planned_quantity};
}
}  // namespace web_bff

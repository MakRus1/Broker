#pragma once
#include <converters/route_projection.hpp>
#include <helpers/database.hpp>
#include <helpers/outbox.hpp>
#include <openapi/models.hpp>
#include <platform/outbox/component.hpp>
#include <userver/formats/json.hpp>

namespace shipments::helpers {
inline void QueueRouteProjection(pg::Transaction& transaction, const std::string& organization,
    const models::Shipment& shipment) {
    clients::transportations::models::ShipmentRouteProjection body{
        .organization_id = organization, .revision = shipment.revision, .shipment_id = shipment.id};
    body.route_segments.reserve(shipment.route_segments.size());
    for (const auto& segment : shipment.route_segments)
        body.route_segments.push_back(converters::ToRouteProjection(segment));

    platform::outbox::EnqueueLatest(transaction, kProjectShipmentRoute,
        organization + ":" + shipment.id,
        userver::formats::json::ValueBuilder(body).ExtractValue());
}
}  // namespace shipments::helpers

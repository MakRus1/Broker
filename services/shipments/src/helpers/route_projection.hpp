#pragma once
#include <converters/route_projection.hpp>
#include <helpers/database.hpp>
#include <helpers/outbox.hpp>
#include <openapi/models.hpp>
#include <platform/outbox/component.hpp>
#include <userver/formats/json.hpp>

namespace shipments::helpers {
inline void QueueRouteProjection(pg::Transaction& transaction,
    const boost::uuids::uuid& organization, const models::Shipment& shipment) {
    clients::transportations::models::ShipmentRouteProjection body{
        .customer_id = shipment.customer_id,
        .organization_id = organization,
        .revision = shipment.revision,
        .shipment_id = shipment.id};
    auto& segments = body.route_segments.emplace();
    segments.reserve(shipment.route_segments.size());
    for (const auto& segment : shipment.route_segments)
        segments.push_back(converters::ToRouteProjection(segment));

    platform::outbox::EnqueueLatest(transaction, kProjectShipmentRoute,
        userver::utils::ToString(organization) + ":" + userver::utils::ToString(shipment.id),
        userver::formats::json::ValueBuilder(body).ExtractValue());
}
}  // namespace shipments::helpers

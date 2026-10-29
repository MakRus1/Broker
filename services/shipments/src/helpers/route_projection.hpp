#pragma once
#include <helpers/database.hpp>
#include <helpers/outbox.hpp>
#include <openapi/models.hpp>
#include <platform/outbox/component.hpp>
#include <userver/formats/json.hpp>

namespace shipments::helpers {
inline void QueueRouteProjection(pg::Transaction& transaction, const std::string& organization,
    const models::Shipment& shipment) {
    userver::formats::json::ValueBuilder body;
    body["organization_id"] = organization;
    body["shipment_id"] = shipment.id;
    body["revision"] = shipment.revision;
    body["route_segments"] = shipment.route_segments;
    platform::outbox::EnqueueLatest(
        transaction, kProjectShipmentRoute, organization + ":" + shipment.id, body.ExtractValue());
}
}  // namespace shipments::helpers

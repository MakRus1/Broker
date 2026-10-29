#pragma once
#include <helpers/database.hpp>
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace shipments::helpers {
inline void QueueRouteProjection(pg::Transaction& transaction, const std::string& organization,
    const models::Shipment& shipment) {
    transaction.Execute(sql::kQueueRouteProjection, organization, shipment.id, shipment.revision,
        userver::formats::json::ValueBuilder(shipment.route_segments).ExtractValue());
}
}  // namespace shipments::helpers

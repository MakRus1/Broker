#pragma once
#include <models/shipment_context.hpp>
#include <openapi/models.hpp>

namespace transportations::route_rules {
bool HasRouteData(const models::RouteSegment&);
const models::ShipmentContext& FindShipmentContext(
    const std::vector<models::ShipmentContext>&, const std::string&);
void InheritRoute(models::Transportation&, const models::ShipmentContext&);
}  // namespace transportations::route_rules

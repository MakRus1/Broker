#pragma once
#include <clients/shipments/client.hpp>
#include <openapi/models.hpp>

namespace transportations::domain {
using ShipmentContext = clients::shipments::models::TransportationContext;
bool HasRouteData(const models::RouteSegment&);
const ShipmentContext& FindShipmentContext(const std::vector<ShipmentContext>&, const std::string&);
void InheritRoute(models::Transportation&, const ShipmentContext&);
}  // namespace transportations::domain

#pragma once
#include <models/shipment_route_references.hpp>

namespace sales::route_rules {
models::ShipmentRouteReferences CollectReferences(const std::vector<models::ShipmentRouteSegment>&);
void ResolveLocations(
    std::vector<models::ShipmentRouteSegment>&, const models::TransportLocations&);
}  // namespace sales::route_rules

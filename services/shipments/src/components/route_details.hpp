#pragma once
#include <map>
#include <openapi/models.hpp>

namespace shipments::route_details {
using RouteLocations = std::map<std::string, models::TransportLocation>;
void ResolveRouteDetails(std::vector<models::ShipmentRouteSegment>& segments,
    const std::vector<models::ShipmentRouteSegment>& previous, const RouteLocations& locations);
}  // namespace shipments::route_details

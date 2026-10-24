#pragma once
#include <map>
#include <openapi/models.hpp>

namespace shipments::domain {
using RouteLocations = std::map<std::string, models::TransportLocation>;
void ResolveRouteDetails(std::vector<models::ShipmentRouteSegment>& segments,
    const std::vector<models::ShipmentRouteSegment>& previous, const RouteLocations& locations);
void ValidateRouteDetails(const std::vector<models::ShipmentRouteSegment>& segments);
}  // namespace shipments::domain

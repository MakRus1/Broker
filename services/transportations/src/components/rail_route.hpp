#pragma once
#include <models/railway_stations.hpp>
#include <openapi/models.hpp>
#include <string>
#include <vector>

namespace transportations::route_rules {
std::vector<std::string> GetRailwayStationIds(const models::SetRouteRequest& request);
void ResolveRailwayStations(models::SetRouteRequest& request,
    const models::Transportation& previous, const models::RailwayStations& stations);
// ЖД-участок хранит отправление и назначение только в rail.
void NormalizeRailLocations(models::RouteSegment& segment);
}  // namespace transportations::route_rules

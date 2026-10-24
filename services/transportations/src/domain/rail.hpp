#pragma once
#include <map>
#include <openapi/models.hpp>
#include <string>
#include <vector>

namespace transportations::domain {
using RailwayStations = std::map<std::string, models::RailwayStation>;
std::vector<std::string> GetRailwayStationCodes(const models::SetRouteRequest& request);
void ResolveRailwayStations(models::SetRouteRequest& request,
    const models::Transportation& previous, const RailwayStations& stations);
// ЖД-участок хранит отправление и назначение только в rail.
void NormalizeRailLocations(models::RouteSegment& segment);
}  // namespace transportations::domain

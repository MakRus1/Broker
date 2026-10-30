#include <algorithm>
#include <domain/rail.hpp>
#include <models/errors.hpp>
#include <set>

namespace transportations::domain {
namespace {
void ResolveStation(std::optional<models::RailwayStation>& station,
    const std::optional<models::RailwayStation>& previous, const RailwayStations& stations) {
    if (!station.has_value())
        return;
    const auto found = stations.find(station.value().id);
    if (found != stations.end()) {
        station = found->second;
        return;
    }
    if (previous.has_value() && previous.value().id == station.value().id) {
        station = previous;
        return;
    }
    throw ApiError<400>{"station_not_found", "Станция не найдена в справочнике станций"};
}
}  // namespace

std::vector<std::string> GetRailwayStationIds(const models::SetRouteRequest& request) {
    std::set<std::string> ids;
    for (const auto& segment : request.route_segments) {
        if (!segment.rail.has_value())
            continue;
        const auto& rail = segment.rail.value();
        if (rail.origin_station.has_value())
            ids.insert(rail.origin_station.value().id);
        if (rail.destination_station.has_value())
            ids.insert(rail.destination_station.value().id);
    }
    return {ids.begin(), ids.end()};
}

void ResolveRailwayStations(models::SetRouteRequest& request,
    const models::Transportation& previous, const RailwayStations& stations) {
    for (auto& segment : request.route_segments) {
        if (!segment.rail.has_value())
            continue;
        const auto old =
            std::find_if(previous.route_segments.begin(), previous.route_segments.end(),
                [&](const auto& value) { return value.id == segment.id; });
        const models::RailDetails empty;
        const auto& saved = old != previous.route_segments.end() && old->rail.has_value()
            ? old->rail.value()
            : empty;
        auto& rail = segment.rail.value();
        ResolveStation(rail.origin_station, saved.origin_station, stations);
        ResolveStation(rail.destination_station, saved.destination_station, stations);
        segment.origin.reset();
        segment.destination.reset();
    }
}

void NormalizeRailLocations(models::RouteSegment& segment) {
    if (segment.mode != models::TransportMode::Rail &&
        segment.mode != models::TransportMode::InternationalRail)
        return;
    segment.origin.reset();
    segment.destination.reset();
}
}  // namespace transportations::domain

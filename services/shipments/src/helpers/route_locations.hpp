#pragma once
#include <domain/route_details.hpp>
#include <helpers/converters/route_details.hpp>
#include <helpers/database.hpp>
#include <set>

namespace shipments::helpers {
inline domain::RouteLocations LoadRouteLocations(
    pg::Cluster& cluster, const std::vector<models::ShipmentRouteSegment>& segments) {
    std::set<boost::uuids::uuid> ids;
    const auto add = [&](const auto& location) {
        if (location.has_value())
            ids.insert(boost::uuids::string_generator{}(location.value().id));
    };
    for (const auto& segment : segments) {
        if (segment.sea.has_value()) {
            add(segment.sea.value().pol);
            add(segment.sea.value().pod);
        }
        if (segment.air.has_value()) {
            add(segment.air.value().origin_airport);
            add(segment.air.value().destination_airport);
        }
    }
    domain::RouteLocations result;
    if (ids.empty())
        return result;
    const auto rows = cluster.Execute(pg::ClusterHostType::kMaster, sql::kGetRouteLocations,
        std::vector<boost::uuids::uuid>{ids.begin(), ids.end()});
    for (const auto& row : rows.AsSetOf<pgtypes::TransportLocationV1>())
        result.emplace(boost::uuids::to_string(row.Id), Convert(row));
    return result;
}
}  // namespace shipments::helpers

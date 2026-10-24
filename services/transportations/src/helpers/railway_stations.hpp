#pragma once
#include <domain/rail.hpp>
#include <helpers/converters.hpp>
#include <helpers/database.hpp>

namespace transportations::helpers {
inline domain::RailwayStations LoadRailwayStations(
    pg::Cluster& cluster, const std::vector<std::string>& codes) {
    domain::RailwayStations stations;
    if (codes.empty())
        return stations;
    const auto rows =
        cluster.Execute(pg::ClusterHostType::kMaster, sql::kGetRailwayStations, codes);
    for (const auto& row : rows.AsSetOf<pgtypes::RailwayStationV1>())
        stations.emplace(row.EsrCode, Convert(row));
    return stations;
}
}  // namespace transportations::helpers

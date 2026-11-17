#pragma once

#include <cstdint>
#include <models/pg/shipment_route_segment.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <vector>

namespace shipments::models {
struct ShipmentRouteSnapshotRow {
    std::int64_t Revision{};
    std::vector<pg::ShipmentRouteSegment> RouteSegments;
    std::int64_t UsageVersion{};
};
}  // namespace shipments::models

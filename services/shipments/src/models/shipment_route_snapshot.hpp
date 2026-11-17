#pragma once

#include <cstdint>
#include <openapi/models.hpp>
#include <vector>

namespace shipments::models {
struct ShipmentRouteSnapshot {
    std::int64_t Revision{};
    std::vector<ShipmentRouteSegment> RouteSegments;
    std::int64_t UsageVersion{};
};
}  // namespace shipments::models

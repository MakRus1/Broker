#pragma once
#include <algorithm>
#include <models/pg/shipment_raw.hpp>
#include <openapi/models.hpp>

namespace shipments::helpers {
inline std::vector<std::string> ChangedRouteSegments(const models::pg::ShipmentRaw& previous,
    const std::vector<models::ShipmentRouteSegment>& next) {
    std::vector<std::string> changed;
    for (const auto& stage : previous.RouteSegments) {
        const auto id = boost::uuids::to_string(stage.Id);
        const auto found =
            std::find_if(next.begin(), next.end(), [&](const auto& item) { return item.id == id; });
        if (found == next.end() || found->type != stage.Type)
            changed.push_back(id);
    }
    return changed;
}

}  // namespace shipments::helpers

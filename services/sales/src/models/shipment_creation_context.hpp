#pragma once
#include <openapi/models.hpp>

namespace sales::models {
struct ShipmentCreationContext {
    std::int64_t InquiryRevision;
    std::optional<std::string> Logist;
    std::vector<ShipmentRouteSegment> RouteSegments;
};
}  // namespace sales::models

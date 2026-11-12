#pragma once
#include <openapi/models.hpp>

namespace transportations::route_rules {
void ValidateRoute(const models::SetRouteRequest&);
void ValidateActualTimes(const models::RouteSegment&);
}  // namespace transportations::route_rules

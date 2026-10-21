#pragma once
#include <openapi/models.hpp>

namespace transportations::domain {
void ValidateRoute(const models::SetRouteRequest&);
void ValidateActualTimes(const models::RouteSegment&);
models::RouteSegment MakeRouteSegment(const models::SetRouteSegmentsItem&);
}  // namespace transportations::domain

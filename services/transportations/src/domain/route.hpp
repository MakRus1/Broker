#pragma once
#include <openapi/models.hpp>

namespace transportations::domain {
void ValidateRoute(const models::SetRouteRequest&);
void ValidateActualTimes(const models::TransportationLeg&);
models::TransportationLeg MakeLeg(const models::SetRouteLegsItem&);
}  // namespace transportations::domain

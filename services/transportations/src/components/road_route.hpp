#pragma once
#include <openapi/models.hpp>

namespace transportations::route_rules {
void ValidateRoadDetails(const models::SetRouteRequest& request);
}

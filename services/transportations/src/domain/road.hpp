#pragma once
#include <openapi/models.hpp>

namespace transportations::domain {
void ValidateRoadDetails(const models::SetRouteRequest& request);
}

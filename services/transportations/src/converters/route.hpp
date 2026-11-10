#pragma once
#include <domain/rail.hpp>
#include <openapi/models.hpp>

namespace transportations::converters {
inline models::RouteSegment Convert(const models::SetRouteSegmentsItem& source) {
    models::RouteSegment result;
    result.rail = source.rail;
    result.international_road = source.international_road;
    result.pickup = source.pickup;
    result.id = source.id;
    result.mode = source.mode;
    if (source.origin.has_value())
        result.origin = models::RouteSegmentOrigin{
            .country_code = source.origin.value().country_code, .name = source.origin.value().name};
    if (source.destination.has_value())
        result.destination =
            models::RouteSegmentDestination{.country_code = source.destination.value().country_code,
                .name = source.destination.value().name};
    domain::NormalizeRailLocations(result);
    result.contractor_id = source.contractor_id;
    result.planned_departure_at = source.planned_departure_at;
    result.planned_arrival_at = source.planned_arrival_at;
    return result;
}
}  // namespace transportations::converters

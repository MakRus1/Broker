#pragma once
#include <components/rail_route.hpp>
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace transportations::converters {
// Морские и авиационные данные принадлежат поставке и не сохраняются в локальном маршруте.
inline userver::formats::json::Value ToStoredRouteSegment(const models::RouteSegment& value) {
    auto local = value;
    if (local.inherited != false) {
        local.plan.reset();
        local.sea.reset();
        local.air.reset();
    }
    return userver::formats::json::ValueBuilder(local).ExtractValue();
}

inline std::vector<userver::formats::json::Value> ToStoredRoute(
    const std::vector<models::RouteSegment>& segments) {
    std::vector<userver::formats::json::Value> documents;
    documents.reserve(segments.size());
    for (const auto& segment : segments)
        documents.push_back(ToStoredRouteSegment(segment));
    return documents;
}

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
    route_rules::NormalizeRailLocations(result);
    result.contractor_id = source.contractor_id;
    result.planned_departure_at = source.planned_departure_at;
    result.planned_arrival_at = source.planned_arrival_at;
    return result;
}
}  // namespace transportations::converters

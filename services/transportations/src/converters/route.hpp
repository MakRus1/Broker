#pragma once
#include <components/rail_route.hpp>
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace transportations::converters {
inline userver::formats::json::Value ToStoredRouteSegment(const models::RouteSegment& value) {
    // InheritRoute выставляет false участкам без источника в поставке.
    const bool is_historical = !value.inherited.value_or(true);
    if (is_historical)
        return userver::formats::json::ValueBuilder(value).ExtractValue();

    models::RouteSegment local;
    local.id = value.id;
    local.mode = value.mode;
    local.origin = value.origin;
    local.destination = value.destination;
    local.contractor_id = value.contractor_id;
    local.planned_departure_at = value.planned_departure_at;
    local.planned_arrival_at = value.planned_arrival_at;
    local.actual_departure_at = value.actual_departure_at;
    local.actual_arrival_at = value.actual_arrival_at;
    local.inherited = value.inherited;
    local.actual_departure_date = value.actual_departure_date;
    local.actual_arrival_date = value.actual_arrival_date;
    local.sea_unloading_date = value.sea_unloading_date;
    local.rail = value.rail;
    local.international_road = value.international_road;
    local.pickup = value.pickup;
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
    result.actual_departure_date = source.actual_departure_date;
    result.actual_arrival_date = source.actual_arrival_date;
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

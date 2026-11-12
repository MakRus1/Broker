#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <components/rail_route.hpp>
#include <components/road_route.hpp>
#include <components/route_dates.hpp>
#include <components/route_validation.hpp>
#include <models/errors.hpp>
#include <set>

namespace {
void CheckInterval(const std::optional<std::chrono::system_clock::time_point>& departure,
    const std::optional<std::chrono::system_clock::time_point>& arrival) {
    if (!departure.has_value() || !arrival.has_value())
        return;
    const bool reversed = departure.value() > arrival.value();
    if (reversed)
        throw transportations::ApiError<400>{"invalid_request", "Отправление позже прибытия"};
}
}  // namespace

namespace transportations::route_rules {
void ValidateRoute(const models::SetRouteRequest& request) {
    ValidateRoadDetails(request);
    const RouteDateValidation dates{"invalid_rail_date", "invalid_rail_sequence",
        "Проверьте последовательность дат ЖД-участка"};
    std::set<boost::uuids::uuid> ids;
    for (const auto& route_segment : request.route_segments) {
        if (!ids.insert(boost::uuids::string_generator{}(route_segment.id)).second)
            throw ApiError<400>{"invalid_request", "ID плеч должны отличаться"};
        CheckInterval(route_segment.planned_departure_at, route_segment.planned_arrival_at);
        if (!route_segment.rail.has_value())
            continue;
        if (route_segment.mode != models::TransportMode::Rail &&
            route_segment.mode != models::TransportMode::InternationalRail)
            throw ApiError<400>{"unexpected_rail_details",
                "Детали ЖД доступны только для железнодорожных участков"};
        const auto& rail = route_segment.rail.value();
        dates.ValidateDate(rail.etd, DateKind::Planned);
        dates.ValidateDate(rail.eta, DateKind::Planned);
        dates.ValidateDate(rail.departure_date, DateKind::Actual);
        dates.ValidateDate(rail.arrival_date, DateKind::Actual);
        dates.ValidateDate(rail.unloading_date, DateKind::Actual);
        dates.ValidateOrder(rail.etd, rail.eta);
        dates.ValidateOrder(rail.departure_date, rail.arrival_date);
        dates.ValidateOrder(rail.arrival_date, rail.unloading_date);
        dates.ValidateOrder(rail.departure_date, rail.unloading_date);
    }
}

void ValidateActualTimes(const models::RouteSegment& route_segment) {
    CheckInterval(route_segment.actual_departure_at, route_segment.actual_arrival_at);
}

}  // namespace transportations::route_rules

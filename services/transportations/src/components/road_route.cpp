#include <components/road_route.hpp>
#include <components/route_dates.hpp>
#include <models/errors.hpp>

namespace transportations::route_rules {
void ValidateRoadDetails(const models::SetRouteRequest& request) {
    const RouteDateValidation dates{"invalid_road_date", "invalid_road_sequence",
        "Проверьте последовательность дат автоучастка"};
    for (const auto& segment : request.route_segments) {
        if (segment.international_road.has_value()) {
            if (segment.mode != models::TransportMode::InternationalRoad)
                throw ApiError<400>{"unexpected_road_details",
                    "Поля международного авто доступны только соответствующему участку"};
            const auto& value = segment.international_road.value();
            dates.ValidateDate(value.planned_loading_date, DateKind::Planned);
            dates.ValidateDate(value.planned_unloading_date, DateKind::Planned);
            dates.ValidateDate(value.departure_date, DateKind::Actual);
            dates.ValidateDate(value.unloading_date, DateKind::Actual);
            dates.ValidateOrder(value.planned_loading_date, value.planned_unloading_date);
            dates.ValidateOrder(value.departure_date, value.unloading_date);
        }
        if (segment.pickup.has_value()) {
            if (segment.mode != models::TransportMode::Road &&
                segment.mode != models::TransportMode::Pickup)
                throw ApiError<400>{
                    "unexpected_pickup_details", "Поля вывоза доступны только участку вывоза"};
            const auto& value = segment.pickup.value();
            dates.ValidateDate(value.planned_pickup_date, DateKind::Planned);
            dates.ValidateDate(value.planned_delivery_date, DateKind::Planned);
            dates.ValidateDate(value.pickup_date, DateKind::Actual);
            dates.ValidateDate(value.delivery_date, DateKind::Actual);
            dates.ValidateDate(value.ttn_date, DateKind::Actual);
            dates.ValidateOrder(value.planned_pickup_date, value.planned_delivery_date);
            dates.ValidateOrder(value.pickup_date, value.delivery_date);
        }
    }
}
}  // namespace transportations::route_rules

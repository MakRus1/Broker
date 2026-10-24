#include <domain/road.hpp>
#include <models/errors.hpp>
#include <platform/date_validation.hpp>
#include <userver/utils/datetime.hpp>

namespace transportations::domain {
namespace {
void CheckDate(const std::optional<std::string>& value, bool actual) {
    if (!value.has_value())
        return;
    using namespace std::chrono;
    const year_month_day maximum = actual
        ? year_month_day{floor<days>(userver::utils::datetime::Now() + hours{3})}
        : year{9999} / December / 31;
    if (!platform::IsCalendarDateInRange(value.value(), year{1900} / January / 1, maximum))
        throw ApiError{400, "invalid_road_date",
            actual ? "Фактическая дата должна быть от 01.01.1900 до сегодняшнего дня"
                   : "Укажите корректную календарную дату от 01.01.1900"};
}

void CheckOrder(const std::optional<std::string>& from, const std::optional<std::string>& to) {
    if (from.has_value() && to.has_value() && from.value() > to.value())
        throw ApiError{
            400, "invalid_road_sequence", "Проверьте последовательность дат автоучастка"};
}
}  // namespace

void ValidateRoadDetails(const models::SetRouteRequest& request) {
    for (const auto& segment : request.route_segments) {
        if (segment.international_road.has_value()) {
            if (segment.mode != models::TransportMode::InternationalRoad)
                throw ApiError{400, "unexpected_road_details",
                    "Поля международного авто доступны только соответствующему участку"};
            const auto& value = segment.international_road.value();
            CheckDate(value.planned_loading_date, false);
            CheckDate(value.planned_unloading_date, false);
            CheckDate(value.departure_date, true);
            CheckDate(value.unloading_date, true);
            CheckOrder(value.planned_loading_date, value.planned_unloading_date);
            CheckOrder(value.departure_date, value.unloading_date);
        }
        if (segment.pickup.has_value()) {
            if (segment.mode != models::TransportMode::Road &&
                segment.mode != models::TransportMode::Pickup)
                throw ApiError{
                    400, "unexpected_pickup_details", "Поля вывоза доступны только участку вывоза"};
            const auto& value = segment.pickup.value();
            CheckDate(value.planned_pickup_date, false);
            CheckDate(value.planned_delivery_date, false);
            CheckDate(value.pickup_date, true);
            CheckDate(value.delivery_date, true);
            CheckDate(value.ttn_date, true);
            CheckOrder(value.planned_pickup_date, value.planned_delivery_date);
            CheckOrder(value.pickup_date, value.delivery_date);
        }
    }
}
}  // namespace transportations::domain

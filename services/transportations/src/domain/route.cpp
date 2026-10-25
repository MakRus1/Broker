#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <domain/rail.hpp>
#include <domain/road.hpp>
#include <domain/route.hpp>
#include <models/errors.hpp>
#include <platform/date_validation.hpp>
#include <set>
#include <userver/utils/datetime.hpp>
#include <userver/utils/datetime_light.hpp>

namespace {
void CheckInterval(
    const std::optional<std::string>& departure, const std::optional<std::string>& arrival) {
    if (!departure.has_value() || !arrival.has_value())
        return;
    auto parse = [](std::string value) {
        if (value.size() < 20)
            throw std::invalid_argument("date-time");
        value[10] = 'T';
        if (value.back() == 'z')
            value.back() = 'Z';
        return userver::utils::datetime::UtcStringtime(
            value, userver::utils::datetime::kRfc3339Format);
    };
    bool reversed;
    try {
        reversed = parse(departure.value()) > parse(arrival.value());
    } catch (const std::exception&) {
        throw transportations::ApiError{400, "invalid_request", "Неподдерживаемая временная метка"};
    }
    if (reversed)
        throw transportations::ApiError{400, "invalid_request", "Отправление позже прибытия"};
}
}  // namespace

namespace transportations::domain {
void ValidateRoute(const models::SetRouteRequest& request) {
    ValidateRoadDetails(request);
    std::set<boost::uuids::uuid> ids;
    for (const auto& route_segment : request.route_segments) {
        if (!ids.insert(boost::uuids::string_generator{}(route_segment.id)).second)
            throw ApiError{400, "invalid_request", "ID плеч должны отличаться"};
        CheckInterval(route_segment.planned_departure_at, route_segment.planned_arrival_at);
        if (!route_segment.rail.has_value())
            continue;
        if (route_segment.mode != models::TransportMode::Rail &&
            route_segment.mode != models::TransportMode::InternationalRail)
            throw ApiError{400, "unexpected_rail_details",
                "Детали ЖД доступны только для железнодорожных участков"};
        const auto& rail = route_segment.rail.value();
        const std::chrono::year_month_day minimum{
            std::chrono::year{1900}, std::chrono::January, std::chrono::day{1}};
        const std::chrono::year_month_day maximum{
            std::chrono::year{9999}, std::chrono::December, std::chrono::day{31}};
        // Календарь бизнесовых событий — Europe/Moscow, без времени в API и БД.
        const std::chrono::year_month_day today{std::chrono::floor<std::chrono::days>(
            userver::utils::datetime::Now() + std::chrono::hours{3})};
        auto check = [&](const std::optional<userver::utils::datetime::Date>& value, bool actual) {
            if (value.has_value() &&
                !platform::IsCalendarDateInRange(value.value(), minimum, actual ? today : maximum))
                throw ApiError{400, "invalid_rail_date",
                    actual ? "Фактическая дата должна быть от 01.01.1900 до сегодняшнего дня"
                           : "Укажите корректную календарную дату от 01.01.1900"};
        };
        check(rail.etd, false);
        check(rail.eta, false);
        check(rail.departure_date, true);
        check(rail.arrival_date, true);
        check(rail.unloading_date, true);
        auto interval = [](const auto& from, const auto& to) {
            if (from.has_value() && to.has_value() && from.value() > to.value())
                throw ApiError{
                    400, "invalid_rail_sequence", "Проверьте последовательность дат ЖД-участка"};
        };
        interval(rail.etd, rail.eta);
        interval(rail.departure_date, rail.arrival_date);
        interval(rail.arrival_date, rail.unloading_date);
        interval(rail.departure_date, rail.unloading_date);
    }
}

void ValidateActualTimes(const models::RouteSegment& route_segment) {
    CheckInterval(route_segment.actual_departure_at, route_segment.actual_arrival_at);
}

models::RouteSegment MakeRouteSegment(const models::SetRouteSegmentsItem& source) {
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
    NormalizeRailLocations(result);
    result.contractor_id = source.contractor_id;
    result.planned_departure_at = source.planned_departure_at;
    result.planned_arrival_at = source.planned_arrival_at;
    return result;
}
}  // namespace transportations::domain

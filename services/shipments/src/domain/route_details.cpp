#include <algorithm>
#include <chrono>
#include <domain/route_details.hpp>
#include <models/errors.hpp>
#include <platform/date_validation.hpp>
#include <userver/utils/datetime.hpp>

namespace shipments::domain {
namespace {
void CheckDate(const std::optional<std::string>& value, bool actual) {
    if (!value.has_value())
        return;
    using namespace std::chrono;
    const year_month_day maximum = actual
        ? year_month_day{floor<days>(userver::utils::datetime::Now() + hours{3})}
        : year{9999} / December / 31;
    if (!platform::IsCalendarDateInRange(value.value(), year{1900} / January / 1, maximum))
        throw ApiError{400, "invalid_route_date",
            actual ? "Фактическая дата должна быть от 01.01.1900 до сегодняшнего дня"
                   : "Укажите корректную календарную дату от 01.01.1900"};
}

void CheckOrder(const std::optional<std::string>& from, const std::optional<std::string>& to) {
    if (from.has_value() && to.has_value() && from.value() > to.value())
        throw ApiError{400, "invalid_route_dates", "Отправление не может быть позже прибытия"};
}

void ResolveLocation(std::optional<models::TransportLocation>& value,
    const std::optional<models::TransportLocation>& previous, models::TransportLocationKind kind,
    const RouteLocations& locations) {
    if (!value.has_value())
        return;
    const auto found = locations.find(value.value().id);
    if (found != locations.end() && found->second.kind == kind) {
        value = found->second;
        return;
    }
    if (previous.has_value() && previous.value().id == value.value().id &&
        previous.value().kind == kind && previous.value().name == value.value().name &&
        previous.value().country_code == value.value().country_code &&
        previous.value().code == value.value().code)
        return;
    throw ApiError{
        400, "route_location_not_found", "Выберите действующий порт или аэропорт из справочника"};
}
}  // namespace

void ValidateRouteDetails(const std::vector<models::ShipmentRouteSegment>& segments) {
    for (const auto& segment : segments) {
        if (segment.sea.has_value()) {
            if (segment.type != "sea")
                throw ApiError{
                    400, "unexpected_sea_details", "Морские поля доступны только участку «Море»"};
            const auto& sea = segment.sea.value();
            CheckDate(sea.eta, false);
            CheckDate(sea.departure_date, true);
        }
        if (segment.air.has_value()) {
            if (segment.type != "air")
                throw ApiError{400, "unexpected_air_details",
                    "Авиационные поля доступны только участку «Авиа»"};
            const auto& air = segment.air.value();
            CheckDate(air.etd, false);
            CheckDate(air.eta, false);
            CheckDate(air.departure_date, true);
            CheckDate(air.arrival_date, true);
            CheckOrder(air.etd, air.eta);
            CheckOrder(air.departure_date, air.arrival_date);
        }
    }
}

void ResolveRouteDetails(std::vector<models::ShipmentRouteSegment>& segments,
    const std::vector<models::ShipmentRouteSegment>& previous, const RouteLocations& locations) {
    ValidateRouteDetails(segments);
    for (auto& segment : segments) {
        const auto found = std::find_if(previous.begin(), previous.end(),
            [&](const auto& old) { return old.id == segment.id && old.type == segment.type; });
        const models::ShipmentRouteSegment empty;
        const auto& old = found == previous.end() ? empty : *found;
        if (segment.sea.has_value()) {
            const models::SeaDetails emptySea;
            const auto& saved = old.sea.has_value() ? old.sea.value() : emptySea;
            auto& sea = segment.sea.value();
            ResolveLocation(sea.pol, saved.pol, models::TransportLocationKind::SeaPort, locations);
            ResolveLocation(sea.pod, saved.pod, models::TransportLocationKind::SeaPort, locations);
        }
        if (segment.air.has_value()) {
            const models::AirDetails emptyAir;
            const auto& saved = old.air.has_value() ? old.air.value() : emptyAir;
            auto& air = segment.air.value();
            ResolveLocation(air.origin_airport, saved.origin_airport,
                models::TransportLocationKind::Airport, locations);
            ResolveLocation(air.destination_airport, saved.destination_airport,
                models::TransportLocationKind::Airport, locations);
        }
    }
}
}  // namespace shipments::domain

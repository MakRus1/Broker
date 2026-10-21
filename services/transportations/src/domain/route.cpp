#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <domain/route.hpp>
#include <models/errors.hpp>
#include <set>
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
    std::set<boost::uuids::uuid> ids;
    for (const auto& leg : request.legs) {
        if (!ids.insert(boost::uuids::string_generator{}(leg.id)).second)
            throw ApiError{400, "invalid_request", "ID плеч должны отличаться"};
        CheckInterval(leg.planned_departure_at, leg.planned_arrival_at);
    }
}

void ValidateActualTimes(const models::TransportationLeg& leg) {
    CheckInterval(leg.actual_departure_at, leg.actual_arrival_at);
}

models::TransportationLeg MakeLeg(const models::SetRouteLegsItem& source) {
    models::TransportationLeg result;
    result.id = source.id;
    result.mode = source.mode;
    result.origin.name = source.origin.name;
    result.origin.country_code = source.origin.country_code;
    result.destination.name = source.destination.name;
    result.destination.country_code = source.destination.country_code;
    result.contractor_id = source.contractor_id;
    result.planned_departure_at = source.planned_departure_at;
    result.planned_arrival_at = source.planned_arrival_at;
    return result;
}
}  // namespace transportations::domain

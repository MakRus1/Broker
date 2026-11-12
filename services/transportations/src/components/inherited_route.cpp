#include <algorithm>
#include <components/inherited_route.hpp>
#include <converters/shipment_route.hpp>
#include <models/errors.hpp>

namespace transportations::route_rules {
using converters::Convert;

bool HasRouteData(const models::RouteSegment& segment) {
    const auto filled = [](const auto& point) {
        return point.has_value() &&
            (!point.value().name.empty() || !point.value().country_code.empty());
    };
    return segment.international_road.has_value() || segment.pickup.has_value() ||
        segment.rail.has_value() || filled(segment.origin) || filled(segment.destination) ||
        segment.contractor_id.has_value() || segment.planned_departure_at.has_value() ||
        segment.planned_arrival_at.has_value() || segment.actual_departure_at.has_value() ||
        segment.actual_arrival_at.has_value();
}

const models::ShipmentContext& FindShipmentContext(
    const std::vector<models::ShipmentContext>& contexts, const std::string& shipment_id) {
    const auto found = std::find_if(contexts.begin(), contexts.end(),
        [&](const auto& item) { return item.shipment_id == shipment_id; });
    if (found == contexts.end())
        throw ApiError<404>{"shipment_not_found", "Поставка не найдена"};
    return *found;
}

void InheritRoute(models::Transportation& transportation, const models::ShipmentContext& source) {
    std::vector<models::RouteSegment> route;
    for (const auto& stage : source.route_segments) {
        const auto found =
            std::find_if(transportation.route_segments.begin(), transportation.route_segments.end(),
                [&](const auto& local) { return local.id == stage.id; });
        auto value = found == transportation.route_segments.end() ? models::RouteSegment{} : *found;
        value.id = stage.id;
        value.inherited = true;
        value.mode = models::ParseEnumScalar(stage.type, models::TransportMode{});
        value.sea.reset();
        value.air.reset();
        if (stage.sea.has_value())
            value.sea = Convert(stage.sea.value());
        if (stage.air.has_value())
            value.air = Convert(stage.air.value());
        route.push_back(std::move(value));
    }
    // Исторические данные самостоятельных маршрутов не теряем при переходе на наследование.
    for (const auto& local : transportation.route_segments) {
        const bool inherited = std::any_of(
            route.begin(), route.end(), [&](const auto& stage) { return stage.id == local.id; });
        if (!inherited && HasRouteData(local)) {
            auto historical = local;
            historical.inherited = false;
            route.push_back(std::move(historical));
        }
    }
    transportation.route_segments = std::move(route);
    transportation.incoterms = source.incoterms;
    transportation.loading = source.loading;
    transportation.carriage_terms = source.carriage_terms;
}

}  // namespace transportations::route_rules

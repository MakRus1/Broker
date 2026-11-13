#include <components/shipment_route.hpp>
#include <models/errors.hpp>

namespace sales::route_rules {
models::ShipmentRouteReferences CollectReferences(
    const std::vector<models::ShipmentRouteSegment>& segments) {
    models::ShipmentRouteReferences result;
    auto& ids = result.LocationIds;

    auto& parties = result.CounterpartyIds;
    const auto addLocation = [&](const auto& value) {
        if (value)
            ids.insert(value->id);
    };
    const auto addParty = [&](const auto& value) {
        if (value)
            parties.insert(*value);
    };
    for (const auto& segment : segments) {
        if (segment.sea) {
            addLocation(segment.sea->pol);
            addLocation(segment.sea->pod);
            addParty(segment.sea->departure_terminal_id);
            addParty(segment.sea->arrival_terminal_id);
        }
        if (segment.air) {
            addLocation(segment.air->origin_airport);
            addLocation(segment.air->destination_airport);
            addParty(segment.air->carrier_id);
        }
    }

    return result;
}

void ResolveLocations(std::vector<models::ShipmentRouteSegment>& segments,
    const models::TransportLocations& locations) {
    const auto resolve = [&](auto& value, models::TransportLocationKind kind) {
        if (!value)
            return;
        const auto found = locations.find(value->id);
        if (found == locations.end() || found->second.kind != kind)
            throw ApiError<400>{
                "invalid_route_location", "Выберите транспортный пункт из справочника"};
        value = found->second;
    };

    for (auto& segment : segments) {
        if (segment.sea) {
            resolve(segment.sea->pol, models::TransportLocationKind::SeaPort);
            resolve(segment.sea->pod, models::TransportLocationKind::SeaPort);
        }
        if (segment.air) {
            resolve(segment.air->origin_airport, models::TransportLocationKind::Airport);
            resolve(segment.air->destination_airport, models::TransportLocationKind::Airport);
        }
    }
}
}  // namespace sales::route_rules

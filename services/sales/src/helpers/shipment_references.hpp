#pragma once
#include <helpers/references.hpp>
#include <map>
#include <set>

namespace sales::helpers {
// Проверяем ссылки с правами автора до фиксации неизменяемой команды.
// Доставка затем использует служебный токен и не зависит от пользовательской сессии.
inline void ResolveShipmentReferences(const Deps& deps,
    const userver::server::request::RequestContext& context,
    std::vector<models::ShipmentRouteSegment>& segments, const std::optional<std::string>& logist) {
    const auto token = platform::auth::GetAccessToken(context);
    if (logist.has_value()) {
        const auto employees = Reference([&] {
            return deps.organizations.ApiOrganizationsEmployeesList(
                {.body = {.limit = 1, .organization_id = Organization(context), .subject = logist}},
                token);
        });
        if (employees.items.empty())
            throw ApiError<400>{
                "invalid_logist", "В карточке клиента назначен недействующий логист"};
    }
    std::set<std::string> ids;
    std::set<std::string> parties;
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
    std::map<std::string, models::TransportLocation> locations;
    if (!ids.empty()) {
        const auto response = Reference([&] {
            return deps.transport_locations.ApiV1RouteLocationsGetBatch(
                {.body = {.ids = std::vector<std::string>(ids.begin(), ids.end())}}, token);
        });
        for (const auto& location : response.items)
            locations.emplace(location.id,
                models::ParseModelJson(json::ValueBuilder(location).ExtractValue(),
                    std::type_identity<models::TransportLocation>{}));
    }
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
    for (const auto& id : parties) {
        const auto response = Reference([&] {
            return deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = id}}, token);
        });
        if (response.counterparty.archived)
            throw ApiError<400>{
                "invalid_route_counterparty", "Выберите действующего контрагента участка"};
    }
}
}  // namespace sales::helpers

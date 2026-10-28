#pragma once
#include <deps.hpp>
#include <domain/route_details.hpp>
#include <helpers/converters/route_details.hpp>
#include <helpers/database.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <set>

namespace shipments::helpers {
inline domain::RouteLocations LoadRouteLocations(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const std::vector<models::ShipmentRouteSegment>& segments) try {
    std::set<boost::uuids::uuid> ids;
    const auto add = [&](const auto& location) {
        if (location.has_value())
            ids.insert(boost::uuids::string_generator{}(location.value().id));
    };
    for (const auto& segment : segments) {
        if (segment.sea.has_value()) {
            add(segment.sea.value().pol);
            add(segment.sea.value().pod);
        }
        if (segment.air.has_value()) {
            add(segment.air.value().origin_airport);
            add(segment.air.value().destination_airport);
        }
    }
    domain::RouteLocations result;
    if (ids.empty())
        return result;
    std::vector<std::string> requested;
    for (const auto& id : ids)
        requested.push_back(boost::uuids::to_string(id));
    const auto response = deps.transport_locations.ApiV1RouteLocationsGetBatch(
        {.body = {.ids = requested}}, platform::auth::GetAccessToken(context));
    for (const auto& value : response.items) {
        models::TransportLocation location{.code = value.code,
            .country_code = value.country_code,
            .id = value.id,
            .kind =
                models::ParseEnumScalar(clients::transport_locations::models::EnumValue(value.kind),
                    models::TransportLocationKind{}),
            .name = value.name};
        result.emplace(location.id, std::move(location));
    }
    return result;
} catch (const platform::http::CancelledError&) {

    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError{
        503, "transport_locations_unavailable", "Не удалось проверить транспортные пункты"};
} catch (const platform::http::TransportError&) {
    throw ApiError{
        503, "transport_locations_unavailable", "Справочник транспортных пунктов недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError{503, "transport_locations_unavailable",
        "Некорректный ответ справочника транспортных пунктов"};
}
}  // namespace shipments::helpers

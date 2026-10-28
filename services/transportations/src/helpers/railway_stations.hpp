#pragma once
#include <deps.hpp>
#include <domain/rail.hpp>
#include <helpers/converters.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>

namespace transportations::helpers {
inline domain::RailwayStations LoadRailwayStations(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const std::vector<std::string>& ids) try {
    domain::RailwayStations stations;
    if (ids.empty())
        return stations;
    const auto response = deps.transport_locations.ApiV1RailwayStationsGetBatch(
        {.body = {.ids = ids}}, platform::auth::GetAccessToken(context));
    for (const auto& value : response.items)
        stations.emplace(value.id,
            models::RailwayStation{.country_code = value.country_code,
                .esr_code = value.esr_code,
                .esr_source = value.esr_source,
                .id = value.id,
                .name = value.name,
                .uic_code = value.uic_code});
    return stations;
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
}  // namespace transportations::helpers

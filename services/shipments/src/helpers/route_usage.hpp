#pragma once
#include <algorithm>
#include <deps.hpp>
#include <helpers/database.hpp>
#include <openapi/models.hpp>
#include <platform/http_client.hpp>
#include <postgresql/types.hpp>

namespace shipments::helpers {
inline std::vector<std::string> ChangedRouteSegments(
    const pgtypes::ShipmentV3& previous, const std::vector<models::ShipmentRouteSegment>& next) {
    std::vector<std::string> changed;
    for (const auto& stage : previous.RouteSegments) {
        const auto id = boost::uuids::to_string(stage.Id);
        const auto found =
            std::find_if(next.begin(), next.end(), [&](const auto& item) { return item.id == id; });
        if (found == next.end() || found->type != stage.Type)
            changed.push_back(id);
    }
    return changed;
}

inline void CheckExistingRouteUsage(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& shipment_id,
    const std::vector<std::string>& changed) try {
    if (changed.empty())
        return;
    namespace api = clients::transportations::ApiV1TransportationRoutesUsage;
    const auto result = deps.transportations.ApiV1TransportationRoutesUsage(
        api::Request{.body = {.shipment_id = shipment_id}},
        platform::auth::GetAccessToken(context));
    for (const auto& id : result.segment_ids)
        if (std::find(changed.begin(), changed.end(), id) != changed.end())
            throw ApiError{409, "route_segment_in_use",
                "Нельзя удалить или изменить тип участка с данными перевозок"};
} catch (const ApiError&) {

    throw;
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const std::exception&) {
    throw ApiError{
        503, "transportations_unavailable", "Не удалось проверить данные участков перевозок"};
}
}  // namespace shipments::helpers

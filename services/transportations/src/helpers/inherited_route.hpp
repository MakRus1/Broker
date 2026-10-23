#pragma once
#include <algorithm>
#include <deps.hpp>
#include <helpers/transportations.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>

namespace transportations::helpers {
using ShipmentContext = clients::shipments::models::TransportationContext;

inline bool HasRouteData(const models::RouteSegment& segment) {
    return !segment.origin.name.empty() || !segment.destination.name.empty() ||
        !segment.origin.country_code.empty() || !segment.destination.country_code.empty() ||
        segment.contractor_id.has_value() || segment.planned_departure_at.has_value() ||
        segment.planned_arrival_at.has_value() || segment.actual_departure_at.has_value() ||
        segment.actual_arrival_at.has_value();
}

inline std::vector<ShipmentContext> LoadShipmentContexts(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const std::vector<std::string>& ids) try {
    namespace api = clients::shipments::ApiV1ShipmentsTransportationContexts;
    if (ids.empty())
        return {};
    return deps.shipments
        .ApiV1ShipmentsTransportationContexts(
            api::Request{.body = {.shipment_ids = ids}}, platform::auth::GetAccessToken(context))
        .items;
} catch (const platform::http::CancelledError&) {

    throw;
} catch (const std::exception&) {
    throw ApiError{503, "shipment_unavailable", "Не удалось получить маршрут поставки"};
}

inline const ShipmentContext& FindShipmentContext(
    const std::vector<ShipmentContext>& contexts, const std::string& shipment_id) {
    const auto found = std::find_if(contexts.begin(), contexts.end(),
        [&](const auto& item) { return item.shipment_id == shipment_id; });
    if (found == contexts.end())
        throw ApiError{404, "shipment_not_found", "Поставка не найдена"};
    return *found;
}

inline void InheritRoute(models::Transportation& transportation, const ShipmentContext& source) {
    std::vector<models::RouteSegment> route;
    for (const auto& stage : source.route_segments) {
        const auto found =
            std::find_if(transportation.route_segments.begin(), transportation.route_segments.end(),
                [&](const auto& local) { return local.id == stage.id; });
        auto value = found == transportation.route_segments.end() ? models::RouteSegment{} : *found;
        value.id = stage.id;
        value.inherited = true;
        value.mode = models::ParseEnumScalar(stage.type, models::TransportMode{});
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

inline void RegisterRouteUsage(const Deps& deps,
    const userver::server::request::RequestContext& context, const ShipmentContext& source,
    const std::vector<std::string>& ids) try {
    if (ids.empty())
        return;
    namespace api = clients::shipments::ApiV1ShipmentsRouteSegmentsRegisterUsage;
    deps.shipments.ApiV1ShipmentsRouteSegmentsRegisterUsage(
        api::Request{.body = {.expected_revision = source.revision,
                         .segment_ids = ids,
                         .shipment_id = source.shipment_id}},
        platform::auth::GetAccessToken(context));
} catch (const platform::http::ApiError& error) {

    if (error.GetStatus() == 409)
        throw ApiError{409, "route_changed", "Маршрут поставки изменён. Обновите перевозку"};
    throw ApiError{503, "shipment_unavailable", "Не удалось защитить участок поставки от удаления"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const std::exception&) {
    throw ApiError{503, "shipment_unavailable", "Не удалось защитить участок поставки от удаления"};
}

inline models::Transportation LoadLocalTransportation(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& id) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
    auto result = ReadTransportation(transaction, organization, id, false);
    transaction.Commit();
    return result;
}

inline models::Transportation LoadTransportation(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& id) {
    auto result = LoadLocalTransportation(deps, context, id);
    const auto contexts = LoadShipmentContexts(deps, context, {result.shipment_id});
    InheritRoute(result, FindShipmentContext(contexts, result.shipment_id));
    return result;
}
}  // namespace transportations::helpers

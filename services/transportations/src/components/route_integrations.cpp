#include <components/inherited_route.hpp>
#include <components/rail_route.hpp>
#include <components/route_integrations.hpp>
#include <converters/railway_station.hpp>
#include <models/errors.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <userver/components/component_context.hpp>

namespace {
std::set<boost::uuids::uuid> RouteCounterparties(const auto& stages) {
    std::set<boost::uuids::uuid> ids;
    for (const auto& stage : stages) {
        if (stage.contractor_id.has_value())
            ids.insert(stage.contractor_id.value());
        if (stage.international_road.has_value()) {
            const auto& road = stage.international_road.value();
            if (road.loading_terminal_id.has_value())
                ids.insert(road.loading_terminal_id.value());
            if (road.unloading_terminal_id.has_value())
                ids.insert(road.unloading_terminal_id.value());
            if (road.customs_terminal_id.has_value())
                ids.insert(road.customs_terminal_id.value());
        }
    }
    return ids;
}

}  // namespace

namespace transportations {
using models::ShipmentContext;

RouteIntegrations::RouteIntegrations(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      shipments_(context.FindComponent<clients::shipments::Component>().GetClient()),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      transport_locations_(
          context.FindComponent<clients::transport_locations::Component>().GetClient()) {}

models::ShipmentContext RouteIntegrations::GetShipmentContext(
    std::string_view access_token, const boost::uuids::uuid& shipment_id) const {
    const auto contexts = FindShipmentContexts(access_token, {shipment_id});
    return route_rules::FindShipmentContext(contexts, shipment_id);
}

std::vector<ShipmentContext> RouteIntegrations::FindShipmentContexts(
    std::string_view access_token, const std::vector<boost::uuids::uuid>& ids) const {
    namespace api = clients::shipments::ApiV1ShipmentsRouteContexts;
    if (ids.empty())
        return {};
    return shipments_
        .ApiV1ShipmentsRouteContexts(api::Request{.body = {.shipment_ids = ids}}, access_token)
        .items;
}

void RouteIntegrations::RegisterRouteUsage(std::string_view access_token,
    const ShipmentContext& source, const std::vector<boost::uuids::uuid>& ids) const try {
    if (ids.empty())
        return;
    namespace api = clients::shipments::ApiV1ShipmentsRouteSegmentsRegisterUsage;
    shipments_.ApiV1ShipmentsRouteSegmentsRegisterUsage(
        api::Request{.body = {.expected_revision = source.revision,
                         .segment_ids = ids,
                         .shipment_id = source.shipment_id}},
        access_token);
} catch (const clients::shipments::ApiV1ShipmentsRouteSegmentsRegisterUsage::Error409& error) {

    throw ApiError<409>{error.response.error, error.response.message};
}

void RouteIntegrations::PrepareRoute(std::string_view access_token,
    models::SetRouteRequest& request, const models::Transportation& previous) const {
    ValidateRouteCounterparties(access_token, request.route_segments, previous.route_segments);

    const auto stations =
        FindRailwayStations(access_token, route_rules::GetRailwayStationIds(request));
    route_rules::ResolveRailwayStations(request, previous, stations);
}

void RouteIntegrations::ValidateRouteCounterparties(std::string_view access_token,
    const std::vector<models::SetRouteSegmentsItem>& stages,
    const std::vector<models::RouteSegment>& previous) const {
    const auto old = RouteCounterparties(previous);
    for (const auto& id : RouteCounterparties(stages)) {
        if (old.contains(id))
            continue;
        try {
            const auto response =
                counterparties_.ApiV1CounterpartiesGet({.body = {.id = id}}, access_token);
            if (response.counterparty.archived)
                throw ApiError<400>{
                    "invalid_route_counterparty", "Выберите действующего контрагента организации"};
        } catch (const platform::http::ErrorByStatus<404>&) {
            throw ApiError<400>{"invalid_route_counterparty", "Контрагент не найден в организации"};
        }
    }
}

models::RailwayStations RouteIntegrations::FindRailwayStations(
    std::string_view access_token, const std::vector<boost::uuids::uuid>& ids) const {
    models::RailwayStations stations;
    if (ids.empty())
        return stations;
    const auto response =
        transport_locations_.ApiV1RailwayStationsGetBatch({.body = {.ids = ids}}, access_token);
    for (const auto& value : response.items)
        stations.emplace(value.id, converters::Convert(value));
    return stations;
}
}  // namespace transportations

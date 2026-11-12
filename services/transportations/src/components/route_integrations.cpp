#include <components/route_integrations.hpp>
#include <converters/railway_station.hpp>
#include <models/errors.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
std::set<std::string> RouteCounterparties(const auto& stages) {
    std::set<std::string> ids;
    for (const auto& stage : stages) {
        if (stage.contractor_id.has_value())
            ids.insert(stage.contractor_id.value());
        if (stage.international_road.has_value() &&
            stage.international_road.value().loading_terminal_id.has_value())
            ids.insert(stage.international_road.value().loading_terminal_id.value());
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

userver::yaml_config::Schema RouteIntegrations::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

std::vector<ShipmentContext> RouteIntegrations::FindShipmentContexts(
    std::string_view access_token, const std::vector<std::string>& ids) const try {
    namespace api = clients::shipments::ApiV1ShipmentsTransportationContexts;
    if (ids.empty())
        return {};
    return shipments_
        .ApiV1ShipmentsTransportationContexts(
            api::Request{.body = {.shipment_ids = ids}}, access_token)
        .items;
} catch (const platform::http::CancelledError&) {

    throw;
} catch (const std::exception&) {
    throw ApiError<503>{"shipment_unavailable", "Не удалось получить маршрут поставки"};
}

void RouteIntegrations::RegisterRouteUsage(std::string_view access_token,
    const ShipmentContext& source, const std::vector<std::string>& ids) const try {
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
} catch (const platform::http::ApiError&) {
    throw ApiError<503>{"shipment_unavailable", "Не удалось защитить участок поставки от удаления"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const std::exception&) {
    throw ApiError<503>{"shipment_unavailable", "Не удалось защитить участок поставки от удаления"};
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
        } catch (const platform::http::ApiError&) {
            throw ApiError<503>{
                "counterparty_unavailable", "Не удалось проверить контрагента участка"};
        } catch (const platform::http::CancelledError&) {
            throw;
        } catch (const platform::http::TransportError&) {
            throw ApiError<503>{
                "counterparty_unavailable", "Не удалось проверить контрагента участка"};
        } catch (const platform::http::ContractError&) {
            throw ApiError<503>{
                "counterparty_unavailable", "Некорректный ответ справочника контрагентов"};
        }
    }
}

models::RailwayStations RouteIntegrations::FindRailwayStations(
    std::string_view access_token, const std::vector<std::string>& ids) const try {
    models::RailwayStations stations;
    if (ids.empty())
        return stations;
    const auto response =
        transport_locations_.ApiV1RailwayStationsGetBatch({.body = {.ids = ids}}, access_token);
    for (const auto& value : response.items)
        stations.emplace(value.id, converters::Convert(value));
    return stations;
} catch (const platform::http::CancelledError&) {

    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<503>{
        "transport_locations_unavailable", "Не удалось проверить транспортные пункты"};
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{
        "transport_locations_unavailable", "Справочник транспортных пунктов недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{
        "transport_locations_unavailable", "Некорректный ответ справочника транспортных пунктов"};
}
}  // namespace transportations

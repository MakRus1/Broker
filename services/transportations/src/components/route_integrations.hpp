#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <models/railway_stations.hpp>
#include <models/shipment_context.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>

namespace transportations {
class RouteIntegrations final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-route-integrations";
    RouteIntegrations(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ShipmentContext GetShipmentContext(
        std::string_view access_token, const std::string& shipment_id) const;
    std::vector<models::ShipmentContext> FindShipmentContexts(
        std::string_view access_token, const std::vector<std::string>&) const;
    void RegisterRouteUsage(std::string_view access_token, const models::ShipmentContext&,
        const std::vector<std::string>&) const;
    void ValidateRouteCounterparties(std::string_view access_token,
        const std::vector<models::SetRouteSegmentsItem>&,
        const std::vector<models::RouteSegment>&) const;
    models::RailwayStations FindRailwayStations(
        std::string_view access_token, const std::vector<std::string>&) const;

  private:
    clients::shipments::Client shipments_;
    clients::counterparties::Client counterparties_;
    clients::transport_locations::Client transport_locations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::RouteIntegrations> = true;

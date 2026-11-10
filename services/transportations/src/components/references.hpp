#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <domain/inherited_route.hpp>
#include <domain/rail.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>

namespace transportations {
class ReferencesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-references";
    ReferencesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<domain::ShipmentContext> LoadShipmentContexts(
        const userver::server::request::RequestContext&, const std::vector<std::string>&) const;
    void RegisterRouteUsage(const userver::server::request::RequestContext&,
        const domain::ShipmentContext&, const std::vector<std::string>&) const;
    void ValidateRouteCounterparties(const userver::server::request::RequestContext&,
        const std::vector<models::SetRouteSegmentsItem>&,
        const std::vector<models::RouteSegment>&) const;
    domain::RailwayStations LoadRailwayStations(
        const userver::server::request::RequestContext&, const std::vector<std::string>&) const;

  private:
    clients::shipments::Client shipments_;
    clients::counterparties::Client counterparties_;
    clients::transport_locations::Client transport_locations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::ReferencesComponent> =
    true;

#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <clients/transportations/client.hpp>
#include <components/route_details.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>

namespace shipments {
class ShipmentCardIntegrations final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipment-card-integrations";
    ShipmentCardIntegrations(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    void ValidateLogist(
        const userver::server::request::RequestContext&, const std::optional<std::string>&) const;
    std::optional<std::string> CustomerLogist(
        const userver::server::request::RequestContext&, const std::string&) const;
    void ResolveRoute(const userver::server::request::RequestContext& context,
        std::vector<models::ShipmentRouteSegment>& segments,
        const std::vector<models::ShipmentRouteSegment>& previous) const;
    void CheckExistingRouteUsage(const userver::server::request::RequestContext&,
        const std::string&, const std::vector<std::string>&) const;

  private:
    route_details::RouteLocations LoadRouteLocations(
        const userver::server::request::RequestContext&,
        const std::vector<models::ShipmentRouteSegment>&) const;
    void ValidateRouteCounterparties(const userver::server::request::RequestContext&,
        const std::vector<models::ShipmentRouteSegment>&,
        const std::vector<models::ShipmentRouteSegment>&) const;
    std::vector<clients::organizations::models::OrganizationEmployee> FindEmployees(
        const userver::server::request::RequestContext&,
        const std::optional<std::string>& = std::nullopt,
        const std::optional<std::string>& = std::nullopt, int = 20) const;
    clients::counterparties::Client counterparties_;
    clients::organizations::Client organizations_;
    clients::transport_locations::Client transport_locations_;
    clients::transportations::Client transportations_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ShipmentCardIntegrations> = true;

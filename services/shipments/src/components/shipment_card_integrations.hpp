#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <clients/transportations/client.hpp>
#include <components/route_details.hpp>
#include <openapi/models.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>

namespace shipments {
class ShipmentCardIntegrations final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipment-card-integrations";
    ShipmentCardIntegrations(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    void ValidateLogist(const std::string& organization, std::string_view access_token,
        const std::optional<std::string>& subject) const;
    std::optional<std::string> CustomerLogist(
        std::string_view access_token, const std::string& customer) const;
    void ResolveRoute(std::string_view access_token,
        std::vector<models::ShipmentRouteSegment>& segments,
        const std::vector<models::ShipmentRouteSegment>& previous) const;
    void CheckExistingRouteUsage(std::string_view access_token, const std::string& shipment_id,
        const std::vector<std::string>& changed_segments) const;

  private:
    route_details::RouteLocations LoadRouteLocations(
        std::string_view access_token, const std::vector<models::ShipmentRouteSegment>&) const;
    void ValidateRouteCounterparties(std::string_view access_token,
        const std::vector<models::ShipmentRouteSegment>&,
        const std::vector<models::ShipmentRouteSegment>&) const;
    std::vector<clients::organizations::models::OrganizationEmployee> FindEmployees(
        const std::string& organization, std::string_view access_token,
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

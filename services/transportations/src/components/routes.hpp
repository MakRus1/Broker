#pragma once
#include <components/route_integrations.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class RoutesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-routes";
    RoutesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RouteUsageResult FindRouteUsage(
        const std::string& organization, const std::string& shipment_id) const;
    models::RouteProjectionResult ApplyShipmentRouteProjection(
        const models::ShipmentRouteProjection&) const;
    models::Transportation SaveRoute(const std::string& organization,
        const platform::auth::Identity&, std::string_view access_token,
        const models::SetRouteRequest&, const models::ShipmentContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    RouteIntegrations& integrations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::RoutesComponent> = true;

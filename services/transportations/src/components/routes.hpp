#pragma once
#include <models/shipment_context.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace transportations {
class RouteIntegrations;

class RoutesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-routes";
    RoutesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    models::RouteUsageResult FindRouteUsage(
        const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment_id) const;
    models::RouteProjectionResult ApplyShipmentRouteProjection(
        const models::ShipmentRouteProjection&) const;
    models::Transportation SaveRoute(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, std::string_view access_token,
        const models::SetRouteRequest&, const models::ShipmentContext&) const;
    // Запись участков в транзакции вызывающей операции, без отдельной редакции и аудита.
    void WriteRoute(userver::storages::postgres::Transaction&,
        const boost::uuids::uuid& organization, std::string_view access_token,
        const std::vector<models::SetRouteSegmentsItem>&, const models::ShipmentContext&,
        models::Transportation&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    RouteIntegrations& integrations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::RoutesComponent> = true;

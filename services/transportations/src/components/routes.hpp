#pragma once
#include <components/references.hpp>
#include <components/transportations.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class RoutesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-routes";
    RoutesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RouteUsageResult Usage(
        models::RouteUsageRequest request, userver::server::request::RequestContext& context) const;
    models::RouteProjectionResult Project(models::ShipmentRouteProjection request,
        userver::server::request::RequestContext& context) const;
    models::TransportationResult Set(
        models::SetRouteRequest request, userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    ReferencesComponent& references_;
    TransportationsComponent& transportations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::RoutesComponent> = true;

#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class ShipmentRegistryComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipment-registry";
    ShipmentRegistryComponent(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ShipmentsListResult FindShipments(const models::ListShipmentsRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentsFilterValuesResult GetShipmentFilterValues(
        const models::ShipmentsFilterValuesRequest& request,
        userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ShipmentRegistryComponent> =
    true;

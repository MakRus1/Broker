#pragma once
#include <components/settings.hpp>
#include <components/shipment_card_integrations.hpp>
#include <models/errors.hpp>
#include <models/shipment_source.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class ShipmentsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-shipments";
    ShipmentsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ShipmentsCreateResult CreateShipment(models::CreateShipmentRequest request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentsGetResult GetShipment(const models::GetShipmentRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentsUpdateResult UpdateShipment(models::UpdateShipmentRequest request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentsTransportationContextsResult GetTransportationContexts(
        const models::TransportationContextsRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentsRegisterRouteUsageResult RegisterRouteUsage(
        const models::RegisterRouteUsageRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentsEnsureCreatedResult EnsureShipmentCreated(
        const models::EnsureShipmentRequest& request,
        userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    SettingsComponent& settings_;
    ShipmentCardIntegrations& card_integrations_;
    std::optional<models::Shipment> FindCreatedShipment(
        const std::string&, const ShipmentSource&, const userver::formats::json::Value&) const;
    models::Shipment CreateShipmentFromSource(const std::string&, const platform::auth::Identity&,
        const models::CreateShipmentRequest&, const ShipmentSource&,
        const userver::formats::json::Value&) const;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ShipmentsComponent> = true;

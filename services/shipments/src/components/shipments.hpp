#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <components/settings.hpp>
#include <models/errors.hpp>
#include <models/shipment_route_snapshot.hpp>
#include <models/shipment_source.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class ShipmentsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-shipments";
    ShipmentsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::optional<std::string> GetCustomerLogist(
        std::string_view access_token, const boost::uuids::uuid& customer_id) const;
    void ValidateLogist(const boost::uuids::uuid& organization, std::string_view access_token,
        const std::optional<std::string>& subject) const;

    models::Shipment GetShipment(
        const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment_id) const;
    std::vector<models::TransportationContext> GetTransportationContexts(
        const boost::uuids::uuid& organization,
        const std::vector<boost::uuids::uuid>& shipment_ids) const;
    models::Shipment UpdateShipment(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, const models::UpdateShipmentRequest&,
        const std::optional<std::int64_t>& route_usage_version) const;
    models::ShipmentRouteSnapshot GetShipmentRouteSnapshot(
        const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment_id) const;
    void RegisterRouteUsage(
        const boost::uuids::uuid& organization, const models::RegisterRouteUsageRequest&) const;
    std::optional<models::Shipment> FindManualShipmentReplay(const boost::uuids::uuid& organization,
        const models::CreateShipmentRequest&, const ShipmentSource&,
        const userver::formats::json::Value&) const;
    std::optional<models::Shipment> FindCreatedShipment(const boost::uuids::uuid&,
        const ShipmentSource&, const userver::formats::json::Value&) const;
    models::Shipment CreateShipmentFromSource(const boost::uuids::uuid&,
        const platform::auth::Identity&, const models::CreateShipmentRequest&,
        const ShipmentSource&, const userver::formats::json::Value&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    SettingsComponent& settings_;
    clients::counterparties::Client counterparties_;
    clients::organizations::Client organizations_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ShipmentsComponent> = true;

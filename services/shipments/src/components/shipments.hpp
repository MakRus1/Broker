#pragma once
#include <components/settings.hpp>
#include <models/errors.hpp>
#include <models/shipment_source.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class ShipmentsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-shipments";
    ShipmentsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Shipment GetShipment(
        const std::string& organization, const std::string& shipment_id) const;
    std::vector<models::Shipment> GetShipments(
        const std::string& organization, const std::vector<std::string>& shipment_ids) const;
    models::Shipment UpdateShipment(const std::string& organization,
        const platform::auth::Identity&, const models::UpdateShipmentRequest&,
        const std::optional<std::int64_t>& route_usage_version) const;
    std::int64_t GetRouteUsageVersion(
        const std::string& organization, const std::string& shipment_id) const;
    void RegisterRouteUsage(
        const std::string& organization, const models::RegisterRouteUsageRequest&) const;
    std::optional<models::Shipment> FindManualShipmentReplay(const std::string& organization,
        const models::CreateShipmentRequest&, const ShipmentSource&,
        const userver::formats::json::Value&) const;
    std::optional<models::Shipment> FindCreatedShipment(
        const std::string&, const ShipmentSource&, const userver::formats::json::Value&) const;
    models::Shipment CreateShipmentFromSource(const std::string&, const platform::auth::Identity&,
        const models::CreateShipmentRequest&, const ShipmentSource&,
        const userver::formats::json::Value&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    SettingsComponent& settings_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ShipmentsComponent> = true;

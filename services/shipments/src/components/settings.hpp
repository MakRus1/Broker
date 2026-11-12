#pragma once
#include <configs/keys.hpp>
#include <models/pg/organization_shipment_rules.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/configs/runtime.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class SettingsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-settings";
    SettingsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    configs::ShipmentSettings Effective(
        const std::string&, const std::optional<std::string>& = std::nullopt) const;
    configs::ShipmentSettings Effective(userver::storages::postgres::Transaction& transaction,
        const std::string& organization, const std::optional<std::string>& customer) const;
    models::ShipmentDictionaries GetDictionaries() const;
    models::ShipmentBusinessSettings GetBusinessSettings(const std::string& organization) const;
    models::ShipmentBusinessSettings SaveBusinessSettings(const std::string& organization,
        const platform::auth::Identity& actor, const models::ShipmentBusinessRules& rules,
        std::int64_t expected_revision) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    platform::configs::Source configs_;
    std::optional<models::pg::OrganizationShipmentRules> LoadRules(const std::string&) const;
    models::ShipmentBusinessSettings BuildBusinessSettings(const configs::ShipmentSettings& limits,
        models::ShipmentBusinessRules rules, std::int64_t revision) const;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::SettingsComponent> = true;

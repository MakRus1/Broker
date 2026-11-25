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
    configs::ShipmentSettings Effective(
        const boost::uuids::uuid&, const std::optional<boost::uuids::uuid>& = std::nullopt) const;
    configs::ShipmentSettings Effective(userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& organization,
        const std::optional<boost::uuids::uuid>& customer) const;
    std::vector<std::string> GetContainerTypes(bool include_inactive = false) const;
    models::ShipmentDictionaries GetDictionaries() const;
    models::ShipmentBusinessSettings GetBusinessSettings(
        const boost::uuids::uuid& organization) const;
    models::ShipmentBusinessSettings SaveBusinessSettings(const boost::uuids::uuid& organization,
        const platform::auth::Identity& actor, const models::ShipmentBusinessRules& rules,
        std::int64_t expected_revision) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    platform::configs::Source configs_;
    std::optional<models::pg::OrganizationShipmentRules> LoadRules(const boost::uuids::uuid&) const;
    models::ShipmentBusinessSettings BuildBusinessSettings(const configs::ShipmentSettings& limits,
        models::ShipmentBusinessRules rules, std::int64_t revision) const;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::SettingsComponent> = true;

#pragma once
#include <components/references.hpp>
#include <configs/keys.hpp>
#include <models/pg/organization_shipment_rules.hpp>
#include <openapi/models.hpp>
#include <platform/configs/runtime.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
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
    models::ShipmentSettings Get(const models::GetShipmentSettingsRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentDictionaries Dictionaries(const models::SettingsDictionariesRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentBusinessSettings GetBusiness(const models::SettingsGetBusinessRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ShipmentBusinessSettings UpdateBusiness(
        const models::SettingsUpdateBusinessRequest& request,
        userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    platform::configs::Source configs_;
    clients::organizations::Client organizations_;
    ReferencesComponent& references_;
    std::optional<models::pg::OrganizationShipmentRules> LoadRules(const std::string&) const;
    models::ShipmentBusinessSettings BuildBusinessSettings(const configs::ShipmentSettings& limits,
        models::ShipmentBusinessRules rules, std::int64_t revision) const;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::SettingsComponent> = true;

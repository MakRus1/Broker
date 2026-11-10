#pragma once
#include <components/references.hpp>
#include <configs/keys.hpp>
#include <openapi/models.hpp>
#include <platform/configs/runtime.hpp>
#include <postgresql/types.hpp>
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
    configs::detail_SHIPMENT_SETTINGS::Value Effective(
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
    configs::detail_SHIPMENT_SETTINGS::Value Platform(
        const std::string&, const std::optional<std::string>& = std::nullopt) const;
    std::optional<pgtypes::OrganizationShipmentRulesV1> LoadRules(const std::string&) const;
    models::ShipmentBusinessSettings Business(const std::string&) const;
    void ValidateBusinessRules(const std::string&, const pgtypes::ShipmentBusinessRulesV1&) const;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::SettingsComponent> = true;

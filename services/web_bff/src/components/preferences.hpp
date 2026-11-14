#pragma once
#include <clients/organizations/client.hpp>
#include <clients/user_preferences/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>

namespace web_bff {
class PreferencesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "workspace-preferences";
    PreferencesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::preferences::ListPreferencesResult GetWorkspacePreferences(
        const std::string& organization, const std::string& token) const;
    models::preferences::Preference SavePersonalPreference(
        const models::preferences::SavePreferenceRequest&, const std::string& token) const;
    models::RegistryDefaultsResult ListRoleRegistryDefaults(
        const std::string& organization, const std::string& token) const;
    models::RegistryDefault SaveRoleRegistryDefault(
        const models::SaveRegistryDefaultRequest&, const std::string& token) const;

  private:
    clients::user_preferences::Client preferences_;
    clients::organizations::Client organizations_;
};
}  // namespace web_bff

template <>
inline constexpr bool userver::components::kHasValidate<web_bff::PreferencesComponent> = true;

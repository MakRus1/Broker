#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace user_preferences {
class PreferencesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "user-preferences";
    PreferencesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<models::Preference> ListPreferences(const models::ListPreferencesRequest&,
        const platform::auth::Identity&, const std::string& access_token) const;
    models::Preference SavePreference(const models::SavePreferenceRequest&,
        const platform::auth::Identity&, const std::string& access_token) const;

  private:
    void CheckOrganization(const std::string& access_token, const std::string&) const;
    userver::storages::postgres::ClusterPtr cluster_;
    clients::organizations::Client organizations_;
};
}  // namespace user_preferences

template <>
inline constexpr bool userver::components::kHasValidate<user_preferences::PreferencesComponent> =
    true;

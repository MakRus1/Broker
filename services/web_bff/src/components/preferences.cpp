#include <components/preferences.hpp>
#include <converters/preferences.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace web_bff {
using converters::Convert;

PreferencesComponent::PreferencesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      preferences_(context.FindComponent<clients::user_preferences::Component>().GetClient()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

userver::yaml_config::Schema PreferencesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::preferences::ListPreferencesResult PreferencesComponent::GetWorkspacePreferences(
    const std::string& organization, const std::string& token) const {
    const auto personal =
        preferences_.ApiV1PreferencesList({.body = {.organization_id = organization}}, token);
    const auto role = organizations_.ApiOrganizationsRegistryDefaultsGet(
        {.body = {.organization_id = organization}}, token);
    models::preferences::ListPreferencesResult result;
    for (const auto& item : personal.preferences)
        result.preferences.push_back(Convert(item));
    for (const auto& item : role.items)
        result.role_defaults.push_back(Convert(item));
    return result;
}

models::preferences::Preference PreferencesComponent::SavePersonalPreference(
    const models::preferences::SavePreferenceRequest& request, const std::string& token) const {
    return Convert(
        preferences_
            .ApiV1PreferencesSave({.body = {.expected_revision = request.expected_revision,
                                       .key = request.key,
                                       .organization_id = request.organization_id,
                                       .value = request.value}},
                token)
            .preference);
}

models::RegistryDefaultsResult PreferencesComponent::ListRoleRegistryDefaults(
    const std::string& organization, const std::string& token) const {
    const auto source = organizations_.ApiOrganizationsRegistryDefaultsList(
        {.body = {.organization_id = organization}}, token);
    models::RegistryDefaultsResult result;
    for (const auto& item : source.items)
        result.items.push_back(Convert(item));
    for (const auto& item : source.roles)
        result.roles.push_back(Convert(item));
    return result;
}

models::RegistryDefault PreferencesComponent::SaveRoleRegistryDefault(
    const models::SaveRegistryDefaultRequest& request, const std::string& token) const {
    return Convert(organizations_
                       .ApiOrganizationsRegistryDefaultsSave(
                           {.body = {.expected_revision = request.expected_revision,
                                .organization_id = request.organization_id,
                                .registry = request.registry,
                                .role = request.role,
                                .value = request.value}},
                           token)
                       .item);
}
}  // namespace web_bff

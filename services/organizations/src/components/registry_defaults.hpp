#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class RegistryDefaultsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-registry-defaults";
    RegistryDefaultsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RegistryDefaultsGetResult GetOwnRegistryDefaults(
        const models::OrganizationIdentity&, const platform::auth::Identity&) const;
    models::RegistryDefaultsListResult ListRoleRegistryDefaults(
        const models::OrganizationIdentity&, const platform::auth::Identity&) const;
    models::RegistryDefaultsSaveResult SaveRoleRegistryDefault(
        const models::RegistryDefaultsSaveRequest&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::RegistryDefaultsComponent> =
    true;

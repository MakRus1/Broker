#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class RolesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-roles";
    RolesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RolesListResult ListRoles(
        const models::OrganizationIdentity&, const platform::auth::Identity&) const;
    models::OrganizationRoleResult CreateRole(
        const models::RolesCreateRequest&, const platform::auth::Identity&) const;
    models::OrganizationRoleResult UpdateRole(
        const models::RolesUpdateRequest&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::RolesComponent> = true;

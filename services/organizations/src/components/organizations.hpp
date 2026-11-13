#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class OrganizationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-organizations";
    OrganizationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::OrganizationsListResult ListOrganizations(
        const models::ListOrganizationsRequest&) const;
    models::OrganizationResult CreateOrganization(
        const models::CreateOrganizationRequest&, const platform::auth::Identity&) const;
    models::OrganizationResult UpdateOrganization(
        const models::UpdateOrganizationRequest&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::OrganizationsComponent> =
    true;

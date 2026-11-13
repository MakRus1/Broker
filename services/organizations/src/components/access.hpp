#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class AccessComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-access";
    AccessComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::OrganizationIdentity CheckMembership(
        const models::OrganizationIdentity&, const platform::auth::Identity&) const;
    models::OrganizationAccess ResolveOrganizationAccess(
        const models::AccessResolveOrganizationRequest&, const platform::auth::Identity&) const;
    models::AccessMineResult FindMyOrganizations(const platform::auth::Identity&) const;
    models::PlatformAccessResult GetPlatformAccess(
        const models::AccessGetPlatformRequest&, const platform::auth::Identity&) const;
    models::PlatformAccessResult UpdatePlatformAccess(
        const models::AccessUpdatePlatformRequest&, const platform::auth::Identity&) const;
    models::PlatformAccess ResolvePlatformAccess(
        const models::AccessResolvePlatformRequest&, const platform::auth::Identity&) const;
    models::AccessEmployeesResult ListEmployees(
        const models::AccessEmployeesRequest&, const platform::auth::Identity&) const;
    models::OrganizationIdentity CheckAdministrator(
        const models::OrganizationIdentity&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::AccessComponent> = true;

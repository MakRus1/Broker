#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class AccessComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-access";
    AccessComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::OrganizationIdentity Membership(
        const models::OrganizationIdentity&, userver::server::request::RequestContext&) const;
    models::OrganizationAccess ResolveOrganization(const models::AccessResolveOrganizationRequest&,
        userver::server::request::RequestContext&) const;
    models::AccessMineResult Mine(userver::server::request::RequestContext&) const;
    models::PlatformAccessResult GetPlatform(
        const models::AccessGetPlatformRequest&, userver::server::request::RequestContext&) const;
    models::PlatformAccessResult UpdatePlatform(const models::AccessUpdatePlatformRequest&,
        userver::server::request::RequestContext&) const;
    models::PlatformAccess ResolvePlatform(const models::AccessResolvePlatformRequest&,
        userver::server::request::RequestContext&) const;
    models::AccessEmployeesResult Employees(
        const models::AccessEmployeesRequest&, userver::server::request::RequestContext&) const;
    models::OrganizationIdentity CheckAdministrator(
        const models::OrganizationIdentity&, userver::server::request::RequestContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::AccessComponent> = true;

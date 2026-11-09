#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class RolesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-roles";
    RolesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RolesListResult List(
        const models::OrganizationIdentity&, userver::server::request::RequestContext&) const;
    models::OrganizationRoleResult Create(
        const models::RolesCreateRequest&, userver::server::request::RequestContext&) const;
    models::OrganizationRoleResult Update(
        const models::RolesUpdateRequest&, userver::server::request::RequestContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::RolesComponent> = true;

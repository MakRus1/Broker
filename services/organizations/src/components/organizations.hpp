#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class OrganizationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-organizations";
    OrganizationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::OrganizationsListResult List(
        const models::ListOrganizationsRequest&, userver::server::request::RequestContext&) const;
    models::OrganizationResult Create(
        const models::CreateOrganizationRequest&, userver::server::request::RequestContext&) const;
    models::OrganizationResult Update(
        const models::UpdateOrganizationRequest&, userver::server::request::RequestContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::OrganizationsComponent> =
    true;

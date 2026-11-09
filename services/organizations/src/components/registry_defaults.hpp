#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class RegistryDefaultsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-registry-defaults";
    RegistryDefaultsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RegistryDefaultsGetResult Get(
        const models::OrganizationIdentity&, userver::server::request::RequestContext&) const;
    models::RegistryDefaultsListResult List(
        const models::OrganizationIdentity&, userver::server::request::RequestContext&) const;
    models::RegistryDefaultsSaveResult Save(const models::RegistryDefaultsSaveRequest&,
        userver::server::request::RequestContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool userver::components::kHasValidate<organizations::RegistryDefaultsComponent> =
    true;

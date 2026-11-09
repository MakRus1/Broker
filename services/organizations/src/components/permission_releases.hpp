#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class PermissionReleasesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-permission-releases";
    PermissionReleasesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::PermissionReleaseResult Upsert(const models::PermissionReleasesUpsertRequest&,
        userver::server::request::RequestContext&) const;
    models::PermissionReleaseResult Activate(const models::PermissionReleasesActivateRequest&,
        userver::server::request::RequestContext&) const;
    models::PermissionReleaseResult Reconcile(const models::PermissionReleasesReconcileRequest&,
        userver::server::request::RequestContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool
    userver::components::kHasValidate<organizations::PermissionReleasesComponent> = true;

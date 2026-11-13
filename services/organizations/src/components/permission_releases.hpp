#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace organizations {
class PermissionReleasesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "organizations-permission-releases";
    PermissionReleasesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::PermissionReleaseResult RegisterPermissionRelease(
        const models::PermissionReleasesUpsertRequest&) const;
    models::PermissionReleaseResult ActivatePermissionRelease(
        const models::PermissionReleasesActivateRequest&) const;
    models::PermissionReleaseResult ReconcilePermissionReleases(
        const models::PermissionReleasesReconcileRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace organizations

template <>
inline constexpr bool
    userver::components::kHasValidate<organizations::PermissionReleasesComponent> = true;

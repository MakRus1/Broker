#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class SettingsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-settings";
    SettingsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::SalesSettings Get(
        const models::ListSalesRequest&, userver::server::request::RequestContext&) const;
    models::SalesSettings GetForOrganization(
        const models::GetSalesSettingsRequest&, userver::server::request::RequestContext&) const;
    models::SalesSettings Update(
        const models::UpdateSalesSettingsRequest&, userver::server::request::RequestContext&) const;

  private:
    void RequireAdministrator(
        const userver::server::request::RequestContext&, const std::string& organization) const;

    struct Dependencies {
        clients::organizations::Client organizations;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::SettingsComponent> = true;

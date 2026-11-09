#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class SettingsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-settings";
    SettingsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::FinanceSettings Get(userver::server::request::RequestContext&) const;
    models::FinanceSettings GetForOrganization(
        const models::GetFinanceSettingsRequest&, userver::server::request::RequestContext&) const;
    models::FinanceSettings Update(const models::UpdateFinanceSettingsRequest&,
        userver::server::request::RequestContext&) const;

  private:
    void RequireAdministrator(
        const userver::server::request::RequestContext&, const std::string&) const;

    struct Dependencies {
        clients::organizations::Client organizations;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::SettingsComponent> = true;

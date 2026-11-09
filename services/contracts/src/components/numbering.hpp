#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace contracts {
class NumberingComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "contracts-numbering";
    NumberingComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ContractNumbering Get(
        const std::string& organization, userver::server::request::RequestContext&) const;
    models::ContractNumbering Update(const models::UpdateContractNumberingRequest&,
        userver::server::request::RequestContext&) const;

  private:
    void RequireAdministrator(
        const userver::server::request::RequestContext&, const std::string&) const;
    clients::organizations::Client organizations_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace contracts

template <>
inline constexpr bool userver::components::kHasValidate<contracts::NumberingComponent> = true;

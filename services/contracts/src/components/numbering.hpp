#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace contracts {
class NumberingComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "contracts-numbering";
    NumberingComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    void RequireAdministrator(const std::string& organization, std::string_view access_token) const;
    models::ContractNumbering GetNumbering(const std::string& organization) const;
    models::ContractNumbering SaveNumbering(const models::UpdateContractNumberingRequest&) const;

  private:
    clients::organizations::Client organizations_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace contracts

template <>
inline constexpr bool userver::components::kHasValidate<contracts::NumberingComponent> = true;

#pragma once
#include <models/contract_filter_values_result.hpp>
#include <models/contract_list_result.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace contracts {
class ContractRegistry final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "contracts-registry";
    ContractRegistry(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ContractListResult ListContracts(
        const std::string& organization, const models::ListContractsRequest&) const;
    models::ContractFilterValuesResult GetContractFilterValues(
        const std::string& organization, const models::ContractFilterValuesRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace contracts

template <>
inline constexpr bool userver::components::kHasValidate<contracts::ContractRegistry> = true;

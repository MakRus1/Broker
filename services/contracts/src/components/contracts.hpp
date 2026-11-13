#pragma once
#include <clients/counterparties/client.hpp>
#include <models/contract_filter_values_result.hpp>
#include <models/contract_list_result.hpp>
#include <models/pg/contract_raw.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace contracts {
class ContractsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "contracts-documents";
    ContractsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::pg::ContractRaw PrepareContract(
        std::string_view access_token, const models::SaveContractRequest&) const;
    models::Contract SaveContract(const std::string& organization,
        const platform::auth::Identity& identity, std::int64_t expected_revision,
        models::pg::ContractRaw value) const;
    models::Contract FindContract(const std::string& organization, const std::string& id) const;
    models::ContractListResult ListContracts(
        const std::string& organization, const models::ListContractsRequest&) const;
    models::ContractFilterValuesResult GetContractFilterValues(
        const std::string& organization, const models::ContractFilterValuesRequest&) const;

  private:
    void ValidateParties(std::string_view access_token, const models::SaveContractRequest&) const;
    clients::counterparties::Client counterparties_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace contracts

template <>
inline constexpr bool userver::components::kHasValidate<contracts::ContractsComponent> = true;

#pragma once
#include <clients/counterparties/client.hpp>
#include <models/pg/contract_raw.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace contracts {
class ContractsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "contracts-contracts";
    ContractsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    void ValidateContractParties(
        std::string_view access_token, const models::SaveContractRequest&) const;
    models::Contract SaveContract(const std::string& organization,
        const platform::auth::Identity& identity, std::int64_t expected_revision,
        models::pg::ContractRaw value) const;
    models::Contract FindContract(const std::string& organization, const std::string& id) const;

  private:
    clients::counterparties::Client counterparties_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace contracts

template <>
inline constexpr bool userver::components::kHasValidate<contracts::ContractsComponent> = true;

#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace counterparties {
class BankAccountsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "counterparties-bank-accounts";
    BankAccountsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::BankAccount SaveBankAccount(const std::string& organization,
        const platform::auth::Identity&, const models::SaveBankAccountRequest&) const;
    std::vector<models::BankAccount> ListBankAccounts(
        const std::string& organization, const models::ListBankAccountsRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace counterparties

template <>
inline constexpr bool userver::components::kHasValidate<counterparties::BankAccountsComponent> =
    true;

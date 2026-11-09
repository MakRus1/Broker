#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace counterparties {
class BankAccountsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "counterparties-bank-accounts";
    BankAccountsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::BankAccount Save(
        const models::SaveBankAccountRequest&, userver::server::request::RequestContext&) const;
    std::vector<models::BankAccount> List(
        const models::ListBankAccountsRequest&, userver::server::request::RequestContext&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace counterparties

template <>
inline constexpr bool userver::components::kHasValidate<counterparties::BankAccountsComponent> =
    true;

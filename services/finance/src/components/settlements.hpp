#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class SettlementsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-settlements";
    SettlementsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ReconciliationResponse GetReconciliation(
        const std::string& organization, const models::ReconciliationRequest& request) const;
    models::CounterpartySettlementsResponse GetBalances(const std::string& organization,
        const models::CounterpartySettlementsRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::SettlementsComponent> = true;

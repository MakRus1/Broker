#pragma once
#include <models/financial_history_access.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class FinancialHistoryComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-history";
    FinancialHistoryComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::FinancialHistoryResponse GetHistory(const std::string& organization,
        const models::FinancialHistoryRequest& body, models::FinancialHistorySource source,
        const models::FinancialHistoryAccess& access) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::FinancialHistoryComponent> = true;

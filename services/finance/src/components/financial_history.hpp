#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class FinancialHistoryComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-history";
    FinancialHistoryComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::FinancialHistoryResponse Load(const userver::server::request::RequestContext&,
        const models::FinancialHistoryRequest&, models::FinancialHistorySource) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::FinancialHistoryComponent> = true;

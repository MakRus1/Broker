#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class SettlementsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-settlements";
    SettlementsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ReconciliationResponse Reconciliation(
        const models::ReconciliationRequest&, userver::server::request::RequestContext&) const;
    models::CounterpartySettlementsResponse Balances(const models::CounterpartySettlementsRequest&,
        userver::server::request::RequestContext&) const;

  private:
    struct Dependencies {
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::SettlementsComponent> = true;

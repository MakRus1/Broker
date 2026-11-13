#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class CardsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-cards";
    CardsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::SalesCard SaveCard(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::SaveSalesCardRequest& request) const;
    models::SalesCard FindCard(const std::string& organization, const std::string& id) const;
    std::vector<models::SalesHistoryEntry> GetCardHistory(
        const std::string& organization, const models::HistoryRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::CardsComponent> = true;

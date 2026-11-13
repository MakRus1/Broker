#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/finance/client.hpp>
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class RfqComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-rfq";
    RfqComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Rfq FindRfq(const std::string& organization, const std::string& id) const;
    std::vector<models::Rfq> ListRfqs(
        const std::string& organization, const std::string& inquiry_id) const;
    std::vector<models::RfqHistoryEntry> GetRfqHistory(
        const std::string& organization, const models::HistoryRequest& request) const;
    // Проверяет клиента и ответственного, заполняет снимки поставщиков и меток в input.
    void PrepareRfq(const std::string& organization, std::string_view access_token,
        const models::Inquiry& inquiry, models::RfqInput& input) const;
    models::Rfq SaveRfq(const std::string& organization, const platform::auth::Identity& identity,
        std::int64_t expected, models::RfqInput input) const;
    models::Rfq FindRfqRevision(
        const std::string& organization, const std::string& id, std::int64_t revision) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    clients::counterparties::Client counterparties_;
    clients::organizations::Client organizations_;
    clients::finance::Client finance_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::RfqComponent> = true;

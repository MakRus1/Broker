#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
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
    models::Rfq SaveRfq(const std::string& organization, const platform::auth::Identity& identity,
        std::int64_t expected, models::RfqInput input) const;
    void ResolveQuote(const std::string& organization, const models::ProposalInput& proposal,
        const models::Inquiry& inquiry, models::ProposalLine& line) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::RfqComponent> = true;

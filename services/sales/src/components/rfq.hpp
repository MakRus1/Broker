#pragma once

#include <clients/counterparties/client.hpp>
#include <clients/finance/client.hpp>
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class RfqComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-rfq";
    RfqComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::Rfq Save(
        const models::SaveRfqRequest&, userver::server::request::RequestContext&) const;
    models::Rfq Get(const std::string& organization, const std::string& id) const;
    std::vector<models::Rfq> List(
        const std::string& organization, const std::string& inquiry_id) const;
    std::vector<models::RfqHistoryEntry> History(
        const std::string& organization, const models::HistoryRequest&) const;
    void ResolveQuote(const std::string& organization, const models::ProposalInput&,
        const models::Inquiry&, models::ProposalLine&) const;

  private:
    struct Dependencies {
        clients::counterparties::Client counterparties;
        clients::finance::Client finance;
        clients::organizations::Client organizations;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::RfqComponent> = true;

#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/finance/client.hpp>
#include <clients/organizations/client.hpp>
#include <components/rfq.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class ProposalsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-proposals";
    ProposalsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Proposal Save(
        const models::SaveProposalRequest&, userver::server::request::RequestContext&) const;
    std::vector<models::Proposal> List(
        const models::IdRequest&, userver::server::request::RequestContext&) const;
    models::Proposal Action(
        const models::ProposalActionRequest&, userver::server::request::RequestContext&) const;
    std::vector<models::ProposalHistoryEntry> History(
        const models::HistoryRequest&, userver::server::request::RequestContext&) const;

  private:
    models::Proposal Calculate(
        const userver::server::request::RequestContext&, models::ProposalInput) const;

    struct Dependencies {
        clients::counterparties::Client counterparties;
        clients::finance::Client finance;
        clients::organizations::Client organizations;
        RfqComponent* rfq;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ProposalsComponent> = true;

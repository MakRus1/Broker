#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class ProposalsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-proposals";
    ProposalsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Proposal SaveProposal(const std::string& organization,
        const platform::auth::Identity& identity, std::int64_t expected_revision,
        models::Proposal value) const;
    std::vector<models::Proposal> ListProposals(
        const std::string& organization, const std::string& inquiry_id) const;
    models::Proposal ApplyProposalAction(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::ProposalActionRequest& request) const;
    std::vector<models::ProposalHistoryEntry> GetProposalHistory(
        const std::string& organization, const models::HistoryRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ProposalsComponent> = true;

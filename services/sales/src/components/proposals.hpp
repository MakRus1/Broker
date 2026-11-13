#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/finance/client.hpp>
#include <models/proposal_calculation.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class RfqComponent;

class ProposalsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-proposals";
    ProposalsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    // Подставляет снимки исходных котировок в input и загружает данные для расчёта.
    models::ProposalCalculation PrepareProposalCalculation(const std::string& organization,
        std::string_view access_token, const models::Inquiry& inquiry,
        models::ProposalInput& input) const;

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
    RfqComponent& rfq_;
    clients::finance::Client finance_;
    clients::counterparties::Client counterparties_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ProposalsComponent> = true;

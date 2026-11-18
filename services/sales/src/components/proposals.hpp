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
    // Подставляет снимки исходных котировок в input и загружает данные для расчёта.
    models::ProposalCalculation PrepareProposalCalculation(const boost::uuids::uuid& organization,
        std::string_view access_token, const models::Inquiry& inquiry,
        models::ProposalInput& input) const;

    models::Proposal SaveProposal(const boost::uuids::uuid& organization,
        const platform::auth::Identity& identity, std::int64_t expected_revision,
        models::Proposal value) const;
    std::vector<models::Proposal> ListProposals(
        const boost::uuids::uuid& organization, const boost::uuids::uuid& inquiry_id) const;
    models::Proposal ApplyProposalAction(const boost::uuids::uuid& organization,
        const platform::auth::Identity& identity,
        const models::ProposalActionRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    RfqComponent& rfq_;
    clients::finance::Client finance_;
    clients::counterparties::Client counterparties_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ProposalsComponent> = true;

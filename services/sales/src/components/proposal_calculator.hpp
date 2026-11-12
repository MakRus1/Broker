#pragma once
#include <clients/finance/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>

namespace sales {
class RfqComponent;
class SalesDirectory;

class ProposalCalculator final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-proposal-calculator";
    ProposalCalculator(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Proposal CalculateProposal(const std::string& organization, std::string_view token,
        const models::Inquiry&, models::ProposalInput) const;

  private:
    clients::finance::Client finance_;
    RfqComponent& rfq_;
    SalesDirectory& directory_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ProposalCalculator> = true;

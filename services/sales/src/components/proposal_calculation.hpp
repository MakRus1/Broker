#pragma once
#include <models/proposal_calculation.hpp>

namespace sales::proposal_rules {
std::int64_t ConvertProposalAmount(std::int64_t amount, std::int64_t rate);
void ValidateInput(const models::ProposalInput&, const models::Inquiry&);
models::Proposal CalculateProposal(models::ProposalInput, const models::ProposalCalculation&);
}  // namespace sales::proposal_rules

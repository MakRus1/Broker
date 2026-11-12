#pragma once
#include <models/proposal_calculation.hpp>

namespace sales::proposal_rules {
void ValidateInput(const models::ProposalInput&, const models::Inquiry&);
models::Proposal CalculateProposal(models::ProposalInput, const models::ProposalCalculation&);
}  // namespace sales::proposal_rules

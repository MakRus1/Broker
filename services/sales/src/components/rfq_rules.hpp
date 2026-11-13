#pragma once
#include <openapi/models.hpp>

namespace sales::rfq_rules {
void ValidateInput(const models::RfqInput&);
void ApplyQuote(const models::ProposalInput&, const models::Inquiry&, const models::Rfq&,
    models::ProposalLine&);
}  // namespace sales::rfq_rules

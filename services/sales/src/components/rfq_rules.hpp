#pragma once
#include <openapi/models.hpp>

namespace sales::rfq_rules {
models::ProposalSuggestionResult SuggestMinimumPurchase(const models::Inquiry&,
    const std::vector<models::Rfq>&, userver::utils::datetime::Date calculation_date);
void ValidateInput(const models::RfqInput&);
void ApplyQuote(const models::ProposalInput&, const models::Inquiry&, const models::Rfq&,
    models::ProposalLine&);
}  // namespace sales::rfq_rules

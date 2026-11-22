#pragma once
#include <map>
#include <openapi/models.hpp>

namespace sales::rfq_rules {
std::vector<models::Rfq> CurrentRfqs(const models::Inquiry&, const std::vector<models::Rfq>&);
models::ProposalSuggestionResult SuggestMinimumPurchase(const models::Inquiry&,
    const std::vector<models::Rfq>&, userver::utils::datetime::Date calculation_date,
    const std::string& reporting_currency, const std::map<std::string, std::int64_t>& rates);
void ValidateInput(const models::RfqInput&);
void ApplyQuote(const models::ProposalInput&, const models::Inquiry&, const models::Rfq&,
    models::ProposalLine&);
}  // namespace sales::rfq_rules

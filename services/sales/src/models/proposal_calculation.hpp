#pragma once
#include <map>
#include <openapi/models.hpp>

namespace sales::models {
struct ProposalCalculation {
    std::int64_t InquiryRevision;
    std::string CustomerName;
    std::string ReportingCurrency;
    std::map<std::string, std::string> LabelNames;
    std::map<std::string, std::string> SupplierNames;
    std::map<std::string, std::int64_t> Rates;
};
}  // namespace sales::models

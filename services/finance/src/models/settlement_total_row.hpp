#pragma once
#include <string>

namespace finance::models {
struct SettlementTotalRow {
    std::string Currency;
    std::string Receivable;
    std::string Payable;
    std::string OverdueReceivable;
    std::string OverduePayable;
    std::string UnallocatedReceipts;
    std::string UnallocatedDisbursements;
};
}  // namespace finance::models

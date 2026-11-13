#pragma once

namespace finance::models {
struct FinancialHistoryAccess {
    bool IncludeRelatedEvents = false;
    bool IncludeInvoiceNames = false;
};
}  // namespace finance::models

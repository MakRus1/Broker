#pragma once
#include <cstdint>
#include <string>

namespace finance::models {
struct FinanceSettingsRow {
    std::string Currency;
    std::int64_t Revision;
    std::string OutgoingNumberTemplate;
    std::string IncomingNumberTemplate;
    bool ResetNumbersYearly;
};
}  // namespace finance::models

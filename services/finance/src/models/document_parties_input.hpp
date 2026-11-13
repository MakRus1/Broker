#pragma once
#include <optional>
#include <string>

namespace finance::models {
struct DocumentPartiesInput {
    std::string OwnCompanyId;
    std::string CounterpartyId;
    std::optional<std::string> OwnBankAccountId;
    std::optional<std::string> CounterpartyBankAccountId;
    std::optional<std::string> ContractId;
    std::string Currency;
    bool Archived = false;
};
}  // namespace finance::models

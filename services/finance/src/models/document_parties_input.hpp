#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>

namespace finance::models {
struct DocumentPartiesInput {
    boost::uuids::uuid OwnCompanyId;
    boost::uuids::uuid CounterpartyId;
    std::optional<boost::uuids::uuid> OwnBankAccountId;
    std::optional<boost::uuids::uuid> CounterpartyBankAccountId;
    std::optional<boost::uuids::uuid> ContractId;
    std::string Currency;
    bool Archived = false;
};
}  // namespace finance::models

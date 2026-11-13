#pragma once
#include <optional>
#include <string>

namespace finance::models {
struct DocumentParties {
    std::string OwnCompanyName;
    std::string CounterpartyName;
    std::optional<std::string> ContractNumber;
};
}  // namespace finance::models

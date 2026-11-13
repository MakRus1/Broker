#pragma once
#include <optional>
#include <string>

namespace finance::models {
struct ServiceChargeParties {
    std::string CounterpartyName;
    std::optional<std::string> ContractNumber;
};
}  // namespace finance::models

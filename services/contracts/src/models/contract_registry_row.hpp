#pragma once
#include <models/pg/contract_raw.hpp>

namespace contracts::models {
struct ContractRegistryRow {
    pg::ContractRaw Contract;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};
}  // namespace contracts::models

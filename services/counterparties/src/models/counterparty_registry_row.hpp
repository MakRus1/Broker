#pragma once
#include <models/pg/counterparty_raw.hpp>

namespace counterparties::models {
struct CounterpartyRegistryRow {
    pg::CounterpartyRaw Counterparty;
    std::string SortKey;
};
}  // namespace counterparties::models

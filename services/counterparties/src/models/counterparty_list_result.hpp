#pragma once
#include <openapi/models.hpp>

namespace counterparties::models {
struct CounterpartyListResult {
    std::vector<Counterparty> items;
    std::optional<CounterpartyRegistryCursor> next_cursor;
};
}  // namespace counterparties::models

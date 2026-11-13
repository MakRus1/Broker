#pragma once
#include <openapi/models.hpp>

namespace counterparties::helpers {
void NormalizeAndValidateBankAccount(models::BankAccount& value);
}  // namespace counterparties::helpers

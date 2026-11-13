#pragma once
#include <openapi/models.hpp>

namespace counterparties::helpers {
models::Counterparty NormalizeAndValidateCounterparty(models::Counterparty value);
}  // namespace counterparties::helpers

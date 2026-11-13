#pragma once
#include <openapi/models.hpp>

namespace contracts::contract_card {
void NormalizeAndValidateContract(models::SaveContractRequest& value);
}  // namespace contracts::contract_card

#pragma once
#include <openapi/models.hpp>

namespace contracts::helpers {
void NormalizeAndValidateContract(models::SaveContractRequest& value);
}  // namespace contracts::helpers

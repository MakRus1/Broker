#pragma once
#include <models/numbering_row.hpp>
#include <openapi/models.hpp>

namespace contracts::converters {
inline models::ContractNumbering Convert(const models::NumberingRow& row) {
    return {.number_template = row.NumberTemplate,
        .reset_number_yearly = row.ResetYearly,
        .revision = row.Revision};
}
}  // namespace contracts::converters

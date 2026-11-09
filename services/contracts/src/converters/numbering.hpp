#pragma once
#include <openapi/models.hpp>

namespace contracts::converters {
struct NumberingRow {
    std::string NumberTemplate;
    bool ResetYearly;
    std::int64_t Revision;
};

inline models::ContractNumbering Convert(const NumberingRow& row) {
    return {.number_template = row.NumberTemplate,
        .reset_number_yearly = row.ResetYearly,
        .revision = row.Revision};
}
}  // namespace contracts::converters

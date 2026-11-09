#pragma once
#include <openapi/models.hpp>
#include <userver/storages/postgres/result_set.hpp>

namespace contracts::helpers {
inline models::ContractNumbering ConvertNumbering(
    const userver::storages::postgres::ResultSet& rows) {
    if (rows.IsEmpty())
        return {
            .number_template = "Д-{sequence}/{yyyy}", .reset_number_yearly = false, .revision = 0};

    struct NumberingRow {
        std::string NumberTemplate;
        bool ResetYearly;
        std::int64_t Revision;
    };

    const auto row = rows.AsSingleRow<NumberingRow>(userver::storages::postgres::kRowTag);
    return {.number_template = row.NumberTemplate,
        .reset_number_yearly = row.ResetYearly,
        .revision = row.Revision};
}
}  // namespace contracts::helpers

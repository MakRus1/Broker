#pragma once
#include <converters/numbering.hpp>
#include <userver/storages/postgres/result_set.hpp>

namespace contracts::helpers {
inline models::ContractNumbering ReadNumbering(const userver::storages::postgres::ResultSet& rows) {
    if (rows.IsEmpty())
        return {
            .number_template = "Д-{sequence}/{yyyy}", .reset_number_yearly = false, .revision = 0};

    return converters::Convert(
        rows.AsSingleRow<converters::NumberingRow>(userver::storages::postgres::kRowTag));
}
}  // namespace contracts::helpers

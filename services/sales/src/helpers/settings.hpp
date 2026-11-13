#pragma once
#include <converters/settings.hpp>
#include <userver/storages/postgres/result_set.hpp>

namespace sales::helpers {
inline models::SalesSettings ReadSalesSettings(const userver::storages::postgres::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.revision = 0, .stages = {}};

    return converters::ToSalesSettings(rows.AsSingleRow<models::pg::SalesSettings>());
}
}  // namespace sales::helpers

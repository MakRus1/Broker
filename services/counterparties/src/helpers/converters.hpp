#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

namespace counterparties::helpers {
inline models::Counterparty Convert(const pgtypes::CounterpartyV1& row) {
    return {.id = boost::uuids::to_string(row.Id), .name = row.Name};
}

inline std::vector<models::Counterparty> Convert(
    const userver::storages::postgres::ResultSet& rows) {
    std::vector<models::Counterparty> items;
    items.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<pgtypes::CounterpartyV1>())
        items.push_back(Convert(row));
    return items;
}
}  // namespace counterparties::helpers

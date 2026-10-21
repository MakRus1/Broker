#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace counterparties::helpers {
inline models::Counterparty Convert(const pgtypes::CounterpartyV1& row) {
    return {.id = boost::uuids::to_string(row.Id), .name = row.Name};
}
}  // namespace counterparties::helpers

#pragma once
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <models/pg/transportation_raw.hpp>
#include <openapi/models.hpp>

namespace transportations::helpers {
enum class ReadMode { Snapshot, ForUpdate };

inline models::Transportation ReadTransportation(pg::Transaction& transaction,
    const boost::uuids::uuid& organization, const boost::uuids::uuid& id, ReadMode mode) {
    const auto rows = transaction.Execute(
        mode == ReadMode::ForUpdate ? sql::kLockTransportation : sql::kGetTransportation,
        organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Перевозка не найдена"};
    return converters::Convert(rows.AsSingleRow<models::pg::TransportationRaw>());
}
}  // namespace transportations::helpers

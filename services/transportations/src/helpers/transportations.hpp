#pragma once
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <models/pg/transportation_raw.hpp>
#include <openapi/models.hpp>

namespace transportations::helpers {
enum class ReadMode { Snapshot, ForUpdate };

inline models::Transportation ReadTransportation(pg::Transaction& transaction,
    const std::string& organization, const std::string& id, ReadMode mode) {
    const auto rows = transaction.Execute(
        mode == ReadMode::ForUpdate ? sql::kLockTransportation : sql::kGetTransportation,
        organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Перевозка не найдена"};
    auto result = converters::Convert(rows.AsSingleRow<models::pg::TransportationRaw>());
    const auto route_segments = transaction.Execute(sql::kGetRouteSegments, organization, id);
    result.route_segments = converters::Convert(route_segments.AsContainer<std::vector<Json>>());
    return result;
}
}  // namespace transportations::helpers

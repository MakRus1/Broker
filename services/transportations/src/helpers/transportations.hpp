#pragma once
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <openapi/models.hpp>

namespace transportations::helpers {
inline models::Transportation ReadTransportation(pg::Transaction& transaction,
    const std::string& organization, const std::string& id, bool lock) {
    const auto rows = transaction.Execute(
        lock ? sql::kLockTransportation : sql::kGetTransportation, organization, id);
    if (rows.IsEmpty())
        throw ApiError{404, "not_found", "Перевозка не найдена"};
    auto result = Convert(rows.AsSingleRow<pgtypes::TransportationV3>());
    const auto route_segments = transaction.Execute(sql::kGetRouteSegments, organization, id);
    result.route_segments = Convert(route_segments.AsContainer<std::vector<Json>>());
    return result;
}
}  // namespace transportations::helpers

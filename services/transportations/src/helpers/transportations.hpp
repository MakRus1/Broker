#pragma once
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <openapi/models.hpp>

namespace transportations::helpers {
inline models::Transportation ReadTransportation(
    pg::Transaction& transaction, const std::string& tenant, const std::string& id, bool lock) {
    const auto rows =
        transaction.Execute(lock ? sql::kLockTransportation : sql::kGetTransportation, tenant, id);
    if (rows.IsEmpty())
        throw ApiError{404, "not_found", "Перевозка не найдена"};
    auto result = Convert(rows.AsSingleRow<pgtypes::TransportationV1>());
    const auto legs = transaction.Execute(sql::kGetLegs, tenant, id);
    for (const auto& document : legs.AsSetOf<Json>())
        result.legs.push_back(
            models::ParseModelJson(document, std::type_identity<models::TransportationLeg>{}));
    return result;
}
}  // namespace transportations::helpers

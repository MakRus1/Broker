#pragma once
#include <repositories/session.hpp>

namespace transportations::storage {
struct TransportationRow {
    std::string Id;
    std::int64_t Revision;
    std::string ShipmentId;
    std::string ItemId;
    std::string OperationId;
    std::string ContainerType;
    std::string Ownership;
};

struct SavedCommand {
    Json Payload;
    Json Result;
};

struct LegRow {
    std::string TransportationId;
    Json Document;
};

inline models::Transportation ToModel(const TransportationRow& row) {
    models::Transportation result;
    result.id = row.Id;
    result.revision = row.Revision;
    result.shipment_id = row.ShipmentId;
    result.item_id = row.ItemId;
    result.operation_id = row.OperationId;
    result.container_type_code = row.ContainerType;
    result.container_ownership =
        models::ParseEnumScalar(row.Ownership, models::ContainerOwnership{});
    return result;
}

inline models::Transportation ReadTransportation(
    Session& session, const std::string& id, bool lock) {
    const auto rows = session.Tx.Execute(
        lock ? sql::kLockTransportation : sql::kGetTransportation, session.Tenant, id);
    if (rows.IsEmpty())
        throw ApiError{404, "not_found", "Перевозка не найдена"};
    auto result = ToModel(rows.AsSingleRow<TransportationRow>(pg::kRowTag));
    const auto legs = session.Tx.Execute(sql::kGetLegs, session.Tenant, id);
    for (const auto& document : legs.AsSetOf<Json>())
        result.legs.push_back(
            models::ParseModelJson(document, std::type_identity<models::TransportationLeg>{}));
    return result;
}
}  // namespace transportations::storage

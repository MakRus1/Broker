#pragma once
#include <helpers/session.hpp>
#include <postgresql/types.hpp>

namespace transportations::helpers {
inline models::Transportation ToModel(const pgtypes::TransportationV1& row) {
    models::Transportation result;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.shipment_id = boost::uuids::to_string(row.ShipmentId);
    result.item_id = boost::uuids::to_string(row.ItemId);
    result.operation_id = boost::uuids::to_string(row.OperationId);
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
    auto result = ToModel(rows.AsSingleRow<pgtypes::TransportationV1>());
    const auto legs = session.Tx.Execute(sql::kGetLegs, session.Tenant, id);
    for (const auto& document : legs.AsSetOf<Json>())
        result.legs.push_back(
            models::ParseModelJson(document, std::type_identity<models::TransportationLeg>{}));
    return result;
}
}  // namespace transportations::helpers

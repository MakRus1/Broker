#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace shipments::helpers {
inline models::Shipment Convert(const pgtypes::ShipmentV1& row) {
    models::Shipment result;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.number = row.Number;
    result.customer_id = boost::uuids::to_string(row.CustomerId);
    result.reference = row.Reference;
    return result;
}

inline models::ShipmentItem Convert(
    const pgtypes::ShipmentItemV1& row, const pgtypes::QuantityTotalsV1& totals) {
    models::ShipmentItem result;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.shipment_id = boost::uuids::to_string(row.ShipmentId);
    result.container_type_code = row.ContainerType;
    result.container_ownership =
        models::ParseEnumScalar(row.Ownership, models::ContainerOwnership{});
    result.planned_quantity = row.Planned;
    result.created_quantity = totals.Created;
    result.pending_quantity = totals.Pending;
    result.available_quantity = row.Planned - totals.Created - totals.Pending;
    return result;
}

inline models::CreationOperation Convert(const pgtypes::CreationOperationV1& row) {
    if (row.Result.has_value())
        return models::ParseModelJson(
            row.Result.value(), std::type_identity<models::CreationOperation>{});
    models::CreationOperation result;
    result.id = boost::uuids::to_string(row.Id);
    result.item_id = boost::uuids::to_string(row.ItemId);
    result.quantity = row.Quantity;
    result.status = models::ParseEnumScalar(row.Status, models::OperationStatus{});
    return result;
}

}  // namespace shipments::helpers

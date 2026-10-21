#pragma once
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace transportations::helpers {
inline models::Transportation Convert(const pgtypes::TransportationV2& row) {
    models::Transportation result;
    result.number = row.Number;
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

}  // namespace transportations::helpers

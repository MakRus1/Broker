#pragma once
#include <boost/uuid/uuid_io.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace transportations::converters {
inline models::ContainerFactsResult Convert(const std::vector<pgtypes::ContainerFactV1>& rows) {
    models::ContainerFactsResult result;
    for (const auto& row : rows) {
        if (row.OperationId.has_value()) {
            result.operations.push_back({.created_quantity = row.Quantity,
                .operation_id = boost::uuids::to_string(row.OperationId.value())});
            continue;
        }
        models::ContainerFact fact;
        fact.container_type_code = row.ContainerType;
        if (row.Ownership.has_value())
            fact.container_ownership =
                models::ParseEnumScalar(row.Ownership.value(), models::ContainerOwnership{});
        fact.quantity = row.Quantity;
        result.groups.push_back(std::move(fact));
    }
    return result;
}
}  // namespace transportations::converters

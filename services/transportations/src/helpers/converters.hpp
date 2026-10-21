#pragma once
#include <map>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

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

inline std::vector<models::TransportationLeg> Convert(
    const std::vector<userver::formats::json::Value>& documents) {
    std::vector<models::TransportationLeg> result;
    result.reserve(documents.size());
    for (const auto& document : documents)
        result.push_back(
            models::ParseModelJson(document, std::type_identity<models::TransportationLeg>{}));
    return result;
}

inline std::vector<models::Transportation> Convert(
    const std::vector<pgtypes::TransportationV2>& rows,
    const std::vector<pgtypes::TransportationLegV1>& legs) {
    std::vector<models::Transportation> result;
    result.reserve(rows.size());
    std::map<boost::uuids::uuid, std::size_t> indexes;
    for (const auto& row : rows) {
        indexes.emplace(row.Id, result.size());
        result.push_back(Convert(row));
    }
    for (const auto& leg : legs)
        result.at(indexes.at(leg.TransportationId))
            .legs.push_back(models::ParseModelJson(
                leg.Document, std::type_identity<models::TransportationLeg>{}));
    return result;
}
}  // namespace transportations::helpers

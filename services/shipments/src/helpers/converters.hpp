#pragma once
#include <helpers/converters/route_details.hpp>
#include <map>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

namespace shipments::helpers {
inline pgtypes::ShipmentDataV2 ConvertData(const pgtypes::ShipmentV3& row) {
    return {.Number = row.Number,
        .CustomerId = row.CustomerId,
        .Reference = row.Reference,
        .ShipmentDate = row.ShipmentDate,
        .RecipientId = row.RecipientId,
        .Logist = row.Logist,
        .CargoDescription = row.CargoDescription,
        .Status = row.Status,
        .Incoterms = row.Incoterms,
        .CarriageTerms = row.CarriageTerms,
        .Loading = row.Loading,
        .DeliveryCity = row.DeliveryCity,
        .DeliveryCountry = row.DeliveryCountry,
        .DeliveryAddress = row.DeliveryAddress,
        .RouteSegments = row.RouteSegments};
}

inline models::Shipment Convert(const pgtypes::ShipmentV3& row) {
    models::Shipment result;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.number = row.Number;
    result.customer_id = boost::uuids::to_string(row.CustomerId);
    result.reference = row.Reference;
    result.shipment_date = row.ShipmentDate;
    result.logist.emplace(row.Logist);
    result.cargo_description.emplace(row.CargoDescription);
    result.status = row.Status;
    result.incoterms.emplace(row.Incoterms);
    result.carriage_terms.emplace(row.CarriageTerms);
    result.loading.emplace(row.Loading);
    result.delivery_city.emplace(row.DeliveryCity);
    result.delivery_country.emplace(row.DeliveryCountry);
    result.delivery_address.emplace(row.DeliveryAddress);
    for (const auto& segment : row.RouteSegments)
        result.route_segments.push_back(Convert(segment));
    if (row.RecipientId.has_value())
        result.recipient_id = boost::uuids::to_string(row.RecipientId.value());
    else
        result.recipient_id.emplace(std::nullopt);
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

inline std::vector<models::Shipment> Convert(const userver::storages::postgres::ResultSet& rows) {
    std::vector<models::Shipment> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<pgtypes::ShipmentV3>())
        result.push_back(Convert(row));
    return result;
}

inline std::vector<models::ShipmentItem> Convert(const std::vector<pgtypes::ShipmentItemV1>& rows,
    const std::map<std::string, pgtypes::QuantityTotalsV1>& totals) {
    std::vector<models::ShipmentItem> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        const auto found = totals.find(boost::uuids::to_string(row.Id));
        result.push_back(
            Convert(row, found == totals.end() ? pgtypes::QuantityTotalsV1{0, 0} : found->second));
    }
    return result;
}
}  // namespace shipments::helpers

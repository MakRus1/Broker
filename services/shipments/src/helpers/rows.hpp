#pragma once
#include <helpers/session.hpp>
#include <postgresql/types.hpp>

namespace shipments::helpers {
inline models::Shipment ToModel(const pgtypes::ShipmentV1& row) {
    models::Shipment result;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.number = row.Number;
    result.customer_id = boost::uuids::to_string(row.CustomerId);
    result.reference = row.Reference;
    return result;
}

inline models::ShipmentItem ToModel(
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

inline pgtypes::QuantityTotalsV1 ReadTotals(Session& session, const std::string& item) {
    return session.Tx.Execute(sql::kOperationTotals, session.Tenant, item)
        .AsSingleRow<pgtypes::QuantityTotalsV1>();
}

inline models::CreationOperation ToModel(const pgtypes::CreationOperationV1& row) {
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

template <typename Result, typename Request>
std::optional<Result> FindReplay(
    Session& session, const std::string& action, const Request& request) {
    session.Tx.Execute(sql::kLockIdempotency, session.Tenant, action, request.idempotency_key);
    const auto rows =
        session.Tx.Execute(sql::kFindIdempotency, session.Tenant, action, request.idempotency_key);
    if (rows.IsEmpty())
        return std::nullopt;
    const auto saved = rows.template AsSingleRow<pgtypes::IdempotentResultV1>();
    if (saved.Request != Document(request))
        throw ApiError{409, "idempotency_conflict", "Ключ уже использован с другим запросом"};
    return models::ParseModelJson(saved.Result, std::type_identity<Result>{});
}

template <typename Request, typename Result>
void SaveReplay(
    Session& session, const std::string& action, const Request& request, const Result& result) {
    session.Tx.Execute(sql::kSaveIdempotency, session.Tenant, action, request.idempotency_key,
        Document(request), Document(result));
}
}  // namespace shipments::helpers

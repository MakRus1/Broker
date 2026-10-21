#pragma once
#include <helpers/session.hpp>

namespace shipments::helpers {
struct ShipmentRow {
    std::string Id;
    std::int64_t Revision;
    std::string Number;
    std::string CustomerId;
    std::string Reference;
};

struct ItemRow {
    std::string Id;
    std::int64_t Revision;
    std::string ShipmentId;
    std::string ContainerType;
    std::string Ownership;
    std::int64_t Planned;
};

struct Totals {
    std::int64_t Created;
    std::int64_t Pending;
};

struct ItemTotals {
    std::string ItemId;
    std::int64_t Created;
    std::int64_t Pending;
};

struct OperationRow {
    std::string Id;
    std::string ItemId;
    std::int64_t Quantity;
    std::string Status;
    std::optional<Json> Result;
};

struct IdempotentRow {
    Json Request;
    Json Result;
};

inline models::Shipment ToModel(const ShipmentRow& row) {
    models::Shipment result;
    result.id = row.Id;
    result.revision = row.Revision;
    result.number = row.Number;
    result.customer_id = row.CustomerId;
    result.reference = row.Reference;
    return result;
}

inline models::ShipmentItem ToModel(const ItemRow& row, const Totals& totals) {
    models::ShipmentItem result;
    result.id = row.Id;
    result.revision = row.Revision;
    result.shipment_id = row.ShipmentId;
    result.container_type_code = row.ContainerType;
    result.container_ownership =
        models::ParseEnumScalar(row.Ownership, models::ContainerOwnership{});
    result.planned_quantity = row.Planned;
    result.created_quantity = totals.Created;
    result.pending_quantity = totals.Pending;
    result.available_quantity = row.Planned - totals.Created - totals.Pending;
    return result;
}

inline Totals ReadTotals(Session& session, const std::string& item) {
    return session.Tx.Execute(sql::kOperationTotals, session.Tenant, item)
        .AsSingleRow<Totals>(pg::kRowTag);
}

inline models::CreationOperation ToModel(const OperationRow& row) {
    if (row.Result.has_value())
        return models::ParseModelJson(
            row.Result.value(), std::type_identity<models::CreationOperation>{});
    models::CreationOperation result;
    result.id = row.Id;
    result.item_id = row.ItemId;
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
    const auto saved = rows.template AsSingleRow<IdempotentRow>(pg::kRowTag);
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

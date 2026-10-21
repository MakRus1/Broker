#include <repositories/rows.hpp>
#include <repositories/store.hpp>

namespace shipments {
using namespace storage;

models::Shipment Store::CreateShipment(const platform::auth::Principal& principal,
    const models::CreateShipmentRequest& request) const {
    Session session{cluster_, principal};
    const auto replay = FindReplay<models::Shipment>(session, "create_shipment", request);
    if (replay.has_value()) {
        session.Tx.Commit();
        return replay.value();
    }
    const auto row = session.Tx
                         .Execute(sql::kCreateShipment, session.Tenant, request.customer_id,
                             request.reference.value_or(""))
                         .AsSingleRow<ShipmentRow>(pg::kRowTag);
    const auto result = ToModel(row);
    SaveReplay(session, "create_shipment", request, result);
    session.Audit("create_shipment", result.id, request, result);
    session.Tx.Commit();
    return result;
}

models::Shipment Store::GetShipment(
    const platform::auth::Principal& principal, const models::GetShipmentRequest& request) const {
    Session session{cluster_, principal, std::nullopt, kReadSnapshot};
    const auto rows = session.Tx.Execute(sql::kGetShipment, session.Tenant, request.shipment_id);
    if (rows.IsEmpty())
        throw ApiError{404, "not_found", "Поставка не найдена"};
    const auto result = ToModel(rows.AsSingleRow<ShipmentRow>(pg::kRowTag));
    session.Tx.Commit();
    return result;
}

std::vector<models::Shipment> Store::ListShipments(
    const platform::auth::Principal& principal, const models::ListShipmentsRequest& request) const {
    Session session{cluster_, principal, std::nullopt, kReadSnapshot};
    const auto rows =
        session.Tx.Execute(sql::kListShipments, session.Tenant, request.after, request.limit);
    std::vector<models::Shipment> result;
    for (const auto& row : rows.AsSetOf<ShipmentRow>(pg::kRowTag))
        result.push_back(ToModel(row));
    session.Tx.Commit();
    return result;
}

models::Shipment Store::UpdateShipment(const platform::auth::Principal& principal,
    const models::UpdateShipmentRequest& request) const {
    Session session{cluster_, principal};
    const auto current =
        session.Tx.Execute(sql::kLockShipment, session.Tenant, request.shipment_id);
    if (current.IsEmpty())
        throw ApiError{404, "not_found", "Поставка не найдена"};
    CheckRevision(
        current.AsSingleRow<ShipmentRow>(pg::kRowTag).Revision, request.expected_revision);
    const auto result =
        ToModel(session.Tx
                    .Execute(sql::kUpdateShipment, session.Tenant, request.shipment_id,
                        request.reference, request.expected_revision)
                    .AsSingleRow<ShipmentRow>(pg::kRowTag));
    session.Audit("update_shipment", result.id, request, result);
    session.Tx.Commit();
    return result;
}
}  // namespace shipments

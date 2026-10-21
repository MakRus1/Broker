#include <map>
#include <repositories/rows.hpp>
#include <repositories/store.hpp>

namespace shipments {
using namespace storage;

models::ShipmentItem Store::AddItem(
    const platform::auth::Principal& principal, const models::AddItemRequest& request) const {
    Session session{cluster_, principal};
    const auto replay = FindReplay<models::ShipmentItem>(session, "add_item", request);
    if (replay.has_value()) {
        session.Tx.Commit();
        return replay.value();
    }
    const auto shipment =
        session.Tx.Execute(sql::kLockShipment, session.Tenant, request.shipment_id);
    if (shipment.IsEmpty())
        throw ApiError{404, "not_found", "Поставка не найдена"};
    CheckRevision(
        shipment.AsSingleRow<ShipmentRow>(pg::kRowTag).Revision, request.expected_revision);
    const auto row =
        session.Tx
            .Execute(sql::kAddItem, session.Tenant, request.shipment_id,
                request.container_type_code, models::EnumValue(request.container_ownership),
                request.planned_quantity)
            .AsSingleRow<ItemRow>(pg::kRowTag);
    session.Tx.Execute(sql::kBumpShipmentRevision, session.Tenant, request.shipment_id);
    const auto result = ToModel(row, Totals{0, 0});
    SaveReplay(session, "add_item", request, result);
    session.Audit("add_item", result.id, request, result);
    session.Tx.Commit();
    return result;
}

std::vector<models::ShipmentItem> Store::ListItems(
    const platform::auth::Principal& principal, const models::ListItemsRequest& request) const {
    Session session{cluster_, principal, std::nullopt, kReadSnapshot};
    if (session.Tx.Execute(sql::kGetShipment, session.Tenant, request.shipment_id).IsEmpty())
        throw ApiError{404, "not_found", "Поставка не найдена"};
    const auto rows = session.Tx
                          .Execute(sql::kListItems, session.Tenant, request.shipment_id,
                              request.after, request.limit)
                          .AsContainer<std::vector<ItemRow>>(pg::kRowTag);
    std::vector<std::string> ids;
    for (const auto& row : rows)
        ids.push_back(row.Id);
    std::map<std::string, Totals> totals;
    if (!ids.empty()) {
        const auto counts = session.Tx.Execute(sql::kListOperationTotals, session.Tenant, ids);
        for (const auto& count : counts.AsSetOf<ItemTotals>(pg::kRowTag))
            totals.emplace(count.ItemId, Totals{count.Created, count.Pending});
    }
    std::vector<models::ShipmentItem> result;
    for (const auto& row : rows) {
        const auto found = totals.find(row.Id);
        result.push_back(ToModel(row, found == totals.end() ? Totals{0, 0} : found->second));
    }
    session.Tx.Commit();
    return result;
}

models::ShipmentItem Store::UpdateItem(
    const platform::auth::Principal& principal, const models::UpdateItemRequest& request) const {
    Session session{cluster_, principal};
    const auto rows = session.Tx.Execute(sql::kLockItem, session.Tenant, request.item_id);
    if (rows.IsEmpty())
        throw ApiError{404, "not_found", "Позиция не найдена"};
    CheckRevision(rows.AsSingleRow<ItemRow>(pg::kRowTag).Revision, request.expected_revision);
    const auto totals = ReadTotals(session, request.item_id);
    if (request.planned_quantity < totals.Created + totals.Pending)
        throw ApiError{409, "quantity_reserved", "Количество уже зарезервировано или создано"};
    const auto row =
        session.Tx
            .Execute(sql::kUpdateItem, session.Tenant, request.item_id, request.planned_quantity)
            .AsSingleRow<ItemRow>(pg::kRowTag);
    const auto result = ToModel(row, totals);
    session.Audit("update_item", result.id, request, result);
    session.Tx.Commit();
    return result;
}
}  // namespace shipments

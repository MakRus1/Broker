#include <map>
#include <repositories/rows.hpp>
#include <repositories/store.hpp>

namespace transportations {
using namespace storage;

models::CreationOperation Store::EnsureCreated(
    const platform::auth::Principal& principal, const models::EnsureCreatedRequest& request) const {
    Session session{cluster_, principal, request.tenant_id};
    session.Tx.Execute(sql::kLockCommand, session.Tenant, request.operation_id);
    const auto rows = session.Tx.Execute(sql::kFindCommand, session.Tenant, request.operation_id);
    if (!rows.IsEmpty()) {
        const auto saved = rows.AsSingleRow<SavedCommand>(pg::kRowTag);
        if (saved.Payload != Document(request))
            throw ApiError{409, "protocol_conflict", "Содержимое операции изменено"};
        const auto result =
            models::ParseModelJson(saved.Result, std::type_identity<models::CreationOperation>{});
        session.Tx.Commit();
        return result;
    }

    models::CreationOperation result;
    result.id = request.operation_id;
    result.item_id = request.item_id;
    result.quantity = request.quantity;
    result.status = models::OperationStatus::Pending;
    session.Tx.Execute(sql::kInsertCommand, session.Tenant, request.operation_id, Document(request),
        Document(result));
    for (std::int64_t ordinal = 1; ordinal <= request.quantity; ++ordinal) {
        result.transportation_ids.push_back(
            session.Tx
                .Execute(sql::kCreateTransportation, session.Tenant, request.shipment_id,
                    request.item_id, request.operation_id, ordinal, request.container_type_code,
                    models::EnumValue(request.container_ownership))
                .AsSingleRow<std::string>());
    }
    result.status = models::OperationStatus::Succeeded;
    session.Tx.Execute(
        sql::kCompleteCommand, session.Tenant, request.operation_id, Document(result));
    session.Audit("ensure_created", result.id, request, result);
    session.Tx.Commit();
    return result;
}

models::Transportation Store::GetTransportation(const platform::auth::Principal& principal,
    const models::GetTransportationRequest& request) const {
    Session session{cluster_, principal, std::nullopt, kReadSnapshot};
    const auto result = ReadTransportation(session, request.transportation_id, false);
    session.Tx.Commit();
    return result;
}

std::vector<models::Transportation> Store::ListTransportations(
    const platform::auth::Principal& principal,
    const models::ListTransportationsRequest& request) const {
    Session session{cluster_, principal, std::nullopt, kReadSnapshot};
    const auto rows = session.Tx.Execute(sql::kListTransportations, session.Tenant, request.after,
        request.shipment_id, request.item_id, request.limit);
    std::vector<models::Transportation> result;
    std::map<std::string, std::size_t> indexes;
    std::vector<std::string> ids;
    for (const auto& row : rows.AsSetOf<TransportationRow>(pg::kRowTag)) {
        indexes.emplace(row.Id, result.size());
        ids.push_back(row.Id);
        result.push_back(ToModel(row));
    }
    // Все плечи страницы читаются одним запросом, без N+1.
    if (!ids.empty()) {
        const auto legs = session.Tx.Execute(sql::kListLegs, session.Tenant, ids);
        for (const auto& leg : legs.AsSetOf<LegRow>(pg::kRowTag))
            result.at(indexes.at(leg.TransportationId))
                .legs.push_back(models::ParseModelJson(
                    leg.Document, std::type_identity<models::TransportationLeg>{}));
    }
    session.Tx.Commit();
    return result;
}
}  // namespace transportations

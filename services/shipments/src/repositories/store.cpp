#include <optional>
#include <repositories/store.hpp>
#include <shipments/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace shipments {
namespace pg = userver::storages::postgres;

Json Store::CreateShipment(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kCreateShipment, "create_shipment", false, true);
}

Json Store::GetShipment(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kGetShipment, "get_shipment", false, false);
}

Json Store::ListShipments(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kListShipments, "list_shipments", false, false);
}

Json Store::UpdateShipment(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kUpdateShipment, "update_shipment", false, true);
}

Json Store::AddItem(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kAddItem, "add_item", false, true);
}

Json Store::ListItems(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kListItems, "list_items", false, false);
}

Json Store::UpdateItem(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kUpdateItem, "update_item", false, true);
}

Json Store::CreateOperation(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kCreateOperation, "create_operation", false, true);
}

Json Store::GetOperation(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kGetOperation, "get_operation", false, false);
}

Json Store::Run(const platform::auth::Principal& principal, const Json& body,
    const pg::Query& query, const std::string& action, bool internal, bool mutation) const {
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto requested =
        internal ? std::optional{body["tenant_id"].As<std::string>()} : std::nullopt;
    const auto rows =
        tx.Execute(sql::kResolveTenant, principal.Issuer, principal.Subject, requested);
    if (rows.Size() != 1)
        throw ApiError{403, "tenant_access_denied", "Не определен доступ к клиенту"};
    const auto tenant = rows.AsSingleRow<std::string>();
    const auto result = tx.Execute(query, tenant, body).AsSingleRow<std::optional<Json>>();
    if (!result)
        throw ApiError{404, "not_found", "Объект не найден"};
    if (result->HasMember("code")) {
        const auto code = (*result)["code"].As<std::string>();
        throw ApiError{code == "invalid_request" ? 400
                : code == "forbidden"            ? 403
                                                 : 409,
            code, (*result)["message"].As<std::string>()};
    }
    if (mutation)
        tx.Execute(sql::kWriteAudit, tenant, principal.ActorId(), action, body, *result);
    tx.Commit();
    return *result;
}
}  // namespace shipments

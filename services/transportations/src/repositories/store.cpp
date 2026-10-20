#include <optional>
#include <repositories/store.hpp>
#include <transportations/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;

Json Store::EnsureCreated(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kEnsureCreated, "ensure_created", true, true);
}

Json Store::GetTransportation(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kGetTransportation, "get_transportation", false, false);
}

Json Store::ListTransportations(
    const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kListTransportations, "list_transportations", false, false);
}

Json Store::SetRoute(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kSetRoute, "set_route", false, true);
}

Json Store::RecordActual(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kRecordActual, "record_actual", false, true);
}

Json Store::CorrectActual(const platform::auth::Principal& principal, const Json& body) const {
    return Run(principal, body, sql::kCorrectActual, "correct_actual", false, true);
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
}  // namespace transportations

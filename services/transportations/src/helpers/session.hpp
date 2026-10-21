#pragma once
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/principal.hpp>
#include <stdexcept>
#include <transportations/sql_queries.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace transportations {
namespace helpers {
namespace pg = userver::storages::postgres;
using Json = userver::formats::json::Value;
inline constexpr pg::TransactionOptions kReadSnapshot{
    pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly};

template <typename T> Json Document(const T& value) {
    return userver::formats::json::ValueBuilder(value).ExtractValue();
}

class Session final {
  public:
    Session(const pg::ClusterPtr& cluster, const platform::auth::Principal& principal,
        const std::optional<std::string>& requested_tenant = std::nullopt,
        pg::TransactionOptions options = {})
        : Tx(cluster->Begin(pg::ClusterHostType::kMaster, options)), Actor(principal.ActorId()) {
        const auto rows =
            Tx.Execute(sql::kResolveTenant, principal.Issuer, principal.Subject, requested_tenant);
        if (rows.Size() != 1)
            throw ApiError{403, "tenant_access_denied", "Не определен доступ к клиенту"};
        Tenant = rows.AsSingleRow<std::string>();
    }

    template <typename Request, typename Result>
    void Audit(const std::string& action, const std::string& id, const Request& request,
        const Result& result) {
        // JSON используется для неизменяемого снимка аудита, а не как интерфейс бизнес-логики.
        userver::formats::json::ValueBuilder document;
        document["request"] = request;
        document["result"] = result;
        Tx.Execute(sql::kWriteAudit, Tenant, Actor, action, id, document.ExtractValue());
    }

    pg::Transaction Tx;
    std::string Tenant;

  private:
    std::string Actor;
};

inline void CheckRevision(std::int64_t actual, std::int64_t expected) {
    if (actual != expected)
        throw ApiError{409, "revision_conflict", "Объект изменен; перечитайте актуальную редакцию"};
}
}  // namespace helpers
}  // namespace transportations

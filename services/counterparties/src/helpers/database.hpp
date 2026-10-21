#pragma once
#include <counterparties/sql_queries.hpp>
#include <models/errors.hpp>
#include <optional>
#include <platform/auth/principal.hpp>
#include <string>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace counterparties {
namespace helpers {
namespace pg = userver::storages::postgres;
inline constexpr pg::TransactionOptions kReadSnapshot{
    pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly};

inline std::string ResolveTenant(pg::Transaction& transaction,
    const platform::auth::Principal& principal,
    const std::optional<std::string>& requested_tenant = std::nullopt) {
    const auto rows = transaction.Execute(
        sql::kResolveTenant, principal.Issuer, principal.Subject, requested_tenant);
    if (rows.Size() != 1)
        throw ApiError{403, "tenant_access_denied", "Не определен доступ к клиенту"};
    return rows.AsSingleRow<std::string>();
}

template <typename Request, typename Result>
void WriteAudit(pg::Transaction& transaction, const std::string& tenant,
    const platform::auth::Principal& principal, const std::string& action, const std::string& id,
    const Request& request, const Result& result) {
    // Снимок аудита сохраняется атомарно с изменением данных.
    userver::formats::json::ValueBuilder document;
    document["request"] = request;
    document["result"] = result;
    transaction.Execute(
        sql::kWriteAudit, tenant, principal.ActorId(), action, id, document.ExtractValue());
}

}  // namespace helpers
}  // namespace counterparties

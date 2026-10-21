#pragma once
#include <counterparties/sql_queries.hpp>
#include <models/errors.hpp>
#include <optional>
#include <platform/auth/identity.hpp>
#include <string>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace counterparties {
namespace helpers {
namespace pg = userver::storages::postgres;

inline std::string ResolveOrganization(pg::Cluster& cluster,
    const platform::auth::Identity& identity,
    const std::optional<std::string>& requested_organization = std::nullopt) {
    const auto rows = cluster.Execute(pg::ClusterHostType::kMaster, sql::kResolveOrganization,
        identity.Issuer, identity.Subject, requested_organization);
    if (rows.Size() != 1)
        throw ApiError{403, "organization_access_denied", "Не определен доступ к клиенту"};
    return rows.AsSingleRow<std::string>();
}

template <typename Request, typename Result>
void WriteAudit(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& identity, const std::string& action, const std::string& id,
    const Request& request, const Result& result) {
    // Снимок аудита сохраняется атомарно с изменением данных.
    userver::formats::json::ValueBuilder document;
    document["request"] = request;
    document["result"] = result;
    transaction.Execute(
        sql::kWriteAudit, organization, identity.ActorId(), action, id, document.ExtractValue());
}

}  // namespace helpers
}  // namespace counterparties

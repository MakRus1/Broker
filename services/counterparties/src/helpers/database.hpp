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

template <typename Request, typename Result>
void WriteAudit(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& identity, const std::string& action, const std::string& id,
    const Request& request, const Result& result,
    const std::optional<Result>& previous = std::nullopt) {
    // Снимок аудита сохраняется атомарно с изменением данных.
    userver::formats::json::ValueBuilder document;
    document["actor_name"] = identity.DisplayName;
    if (previous.has_value())
        document["before"] = previous.value();
    document["request"] = request;
    document["result"] = result;
    transaction.Execute(
        sql::kWriteAudit, organization, identity.ActorId(), action, id, document.ExtractValue());
}

}  // namespace helpers
}  // namespace counterparties

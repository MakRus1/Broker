#pragma once
#include <models/errors.hpp>
#include <optional>
#include <platform/auth/identity.hpp>
#include <string>
#include <transportations/sql_queries.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace transportations {
namespace helpers {
namespace pg = userver::storages::postgres;

template <typename Request, typename Result>
void WriteAudit(pg::Transaction& transaction, const boost::uuids::uuid& organization,
    const platform::auth::Identity& identity, const std::string& action,
    const boost::uuids::uuid& id, const Request& request, const Result& result) {
    // Снимок аудита сохраняется атомарно с изменением данных.
    userver::formats::json::ValueBuilder document;
    document["actor_name"] = identity.DisplayName;
    document["request"] = request;
    document["result"] = result;
    transaction.Execute(
        sql::kWriteAudit, organization, identity.ActorId(), action, id, document.ExtractValue());
}

}  // namespace helpers
}  // namespace transportations

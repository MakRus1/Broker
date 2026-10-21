#pragma once
#include <helpers/access.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace organizations::helpers {
template <typename Value>
void WriteAudit(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& identity, const std::string& action, const Value& value) {
    transaction.Execute(sql::kWriteAudit, organization, identity.ActorId(), action,
        userver::formats::json::ValueBuilder(value).ExtractValue());
}
}  // namespace organizations::helpers

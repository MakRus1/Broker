#pragma once
#include <deps.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <sales/sql_queries.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace sales::helpers {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

template <typename T> T Read(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    return models::ParseModelJson(rows.AsSingleRow<json::Value>(), std::type_identity<T>{});
}

template <typename T> std::vector<T> ReadList(const pg::ResultSet& rows) {
    std::vector<T> items;
    items.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        items.push_back(models::ParseModelJson(row, std::type_identity<T>{}));
    return items;
}

template <typename T>
void Audit(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& actor, const std::string& entity, const T& value) {
    transaction.Execute(sql::kWriteHistory, organization, entity, value.id, value.revision,
        actor.ActorId(), json::ValueBuilder(value).ExtractValue());
}

inline std::string Organization(const userver::server::request::RequestContext& context) {
    return platform::auth::GetOrganization(platform::auth::GetIdentity(context));
}
}  // namespace sales::helpers

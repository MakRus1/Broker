#pragma once
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <sales/sql_queries.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace sales::helpers {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

inline models::Proposal ReadProposal(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        throw ApiError<404>{"proposal_not_found", "Предложение не найдено"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Proposal>{});
}

}  // namespace sales::helpers

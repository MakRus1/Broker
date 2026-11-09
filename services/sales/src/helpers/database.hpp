#pragma once
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

}  // namespace sales::helpers

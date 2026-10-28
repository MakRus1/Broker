#pragma once
#include <postgresql/types.hpp>
#include <transport_locations/sql_queries.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace transport_locations::helpers {
namespace pg = userver::storages::postgres;
inline constexpr pg::TransactionOptions kReadSnapshot{
    pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly};
}  // namespace transport_locations::helpers

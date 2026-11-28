#pragma once
#include <userver/storages/postgres/transaction.hpp>

namespace platform::db {
// Согласованное чтение нескольких запросов без записи.
inline constexpr userver::storages::postgres::TransactionOptions kReadOnlySnapshot{
    userver::storages::postgres::IsolationLevel::kRepeatableRead,
    userver::storages::postgres::TransactionOptions::kReadOnly};
}  // namespace platform::db

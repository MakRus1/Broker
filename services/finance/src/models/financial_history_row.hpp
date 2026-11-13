#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace finance::models {
struct FinancialHistoryRow {
    std::string Source;
    boost::uuids::uuid EntityId;
    std::int64_t Revision;
    std::string Actor;
    userver::storages::postgres::TimePointTz OccurredAt;
    userver::formats::json::Value Snapshot;
    std::optional<userver::formats::json::Value> Previous;
};
}  // namespace finance::models

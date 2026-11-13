#pragma once
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace notifications::models {
struct NotificationRow {
    boost::uuids::uuid Id;
    boost::uuids::uuid EntityId;
    std::string EntityNumber;
    std::int64_t EntityRevision;
    userver::storages::postgres::TimePointTz CreatedAt;
    bool Read;
    std::string EventType;
};
}  // namespace notifications::models

#pragma once

#include <models/json.hpp>
#include <string>
#include <userver/storages/postgres/io/chrono.hpp>

namespace configs {
struct Revision {
    std::int64_t Id;
    Json Values;
    userver::storages::postgres::TimePointTz PublishedAt;
};

struct RevisionInfo {
    std::int64_t Id;
    userver::storages::postgres::TimePointTz CreatedAt;
};
}  // namespace configs

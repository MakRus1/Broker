#pragma once

#include <models/draft.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace configs {
struct DraftEvent {
    std::string Action;
    std::string Actor;
    Draft Snapshot;
    userver::storages::postgres::TimePointTz CreatedAt;
};
}  // namespace configs

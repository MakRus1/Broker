#pragma once

#include <models/json.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/chrono.hpp>

namespace configs {
struct ConfigValues {
    Json Values;
    std::int64_t Revision;
    userver::storages::postgres::TimePointTz PublishedAt;
};

struct CatalogEntry {
    std::string Name;
    std::int64_t Version;
    std::optional<std::int64_t> ActiveDraftId;
};

struct Config {
    std::string Name;
    std::optional<Json> Value;
    std::int64_t Version;
    std::optional<std::int64_t> ActiveDraftId;
};
}  // namespace configs

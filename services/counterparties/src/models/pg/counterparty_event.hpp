#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace counterparties::models::pg {
struct CounterpartyEvent {
    std::int64_t Id{};
    std::string Actor{};
    std::string Action{};
    userver::storages::postgres::TimePointTz OccurredAt{};
    userver::formats::json::Value Document{};
    std::optional<userver::formats::json::Value> Previous{};
};
}  // namespace counterparties::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::counterparties::models::pg::CounterpartyEvent> {
    static constexpr DBTypeName postgres_name = "public.counterparty_event_v2";
};

}  // namespace userver::storages::postgres::io

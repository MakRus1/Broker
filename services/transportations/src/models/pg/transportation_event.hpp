#pragma once
#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace transportations::models::pg {
struct TransportationEvent {
    std::int64_t Id{};
    std::string Actor{};
    std::string Action{};
    userver::storages::postgres::TimePointTz OccurredAt{};
    userver::formats::json::Value Document{};
};
}  // namespace transportations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transportations::models::pg::TransportationEvent> {
    static constexpr DBTypeName postgres_name = "public.transportation_event_v2";
};

}  // namespace userver::storages::postgres::io

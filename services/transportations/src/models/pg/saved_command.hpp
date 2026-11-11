#pragma once
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace transportations::models::pg {
struct SavedCommand {
    userver::formats::json::Value Payload{};
    userver::formats::json::Value Result{};
};
}  // namespace transportations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transportations::models::pg::SavedCommand> {
    static constexpr DBTypeName postgres_name = "public.saved_command_v1";
};

}  // namespace userver::storages::postgres::io

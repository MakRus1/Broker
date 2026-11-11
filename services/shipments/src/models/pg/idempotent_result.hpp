#pragma once
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace shipments::models::pg {
struct IdempotentResult {
    userver::formats::json::Value Request{};
    userver::formats::json::Value Result{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::IdempotentResult> {
    static constexpr DBTypeName postgres_name = "public.idempotent_result_v1";
};

}  // namespace userver::storages::postgres::io

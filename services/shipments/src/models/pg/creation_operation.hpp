#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace shipments::models::pg {
struct CreationOperation {
    boost::uuids::uuid Id{};
    boost::uuids::uuid ItemId{};
    std::int64_t Quantity{};
    std::string Status{};
    std::optional<userver::formats::json::Value> Result{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::CreationOperation> {
    static constexpr DBTypeName postgres_name = "public.creation_operation_v1";
};

}  // namespace userver::storages::postgres::io

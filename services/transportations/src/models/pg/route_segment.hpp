#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transportations::models::pg {
struct RouteSegment {
    boost::uuids::uuid TransportationId{};
    userver::formats::json::Value Document{};
};
}  // namespace transportations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transportations::models::pg::RouteSegment> {
    static constexpr DBTypeName postgres_name = "public.route_segment_v1";
};

}  // namespace userver::storages::postgres::io

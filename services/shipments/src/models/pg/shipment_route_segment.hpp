#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <models/pg/air_details.hpp>
#include <models/pg/sea_details.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace shipments::models::pg {
struct ShipmentRouteSegment {
    boost::uuids::uuid Id{};
    std::string Type{};
    std::optional<SeaDetails> Sea{};
    std::optional<AirDetails> Air{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::ShipmentRouteSegment> {
    static constexpr DBTypeName postgres_name = "public.shipment_route_segment_v3";
};

}  // namespace userver::storages::postgres::io

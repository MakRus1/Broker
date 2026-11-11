#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace shipments::models::pg {
struct ShipmentItem {
    boost::uuids::uuid Id{};
    std::int64_t Revision{};
    boost::uuids::uuid ShipmentId{};
    std::string ContainerType{};
    std::string Ownership{};
    std::int64_t Planned{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::ShipmentItem> {
    static constexpr DBTypeName postgres_name = "public.shipment_item_v1";
};

}  // namespace userver::storages::postgres::io

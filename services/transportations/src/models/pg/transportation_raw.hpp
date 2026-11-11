#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transportations::models::pg {
struct TransportationRaw {
    boost::uuids::uuid Id{};
    std::int64_t Revision{};
    boost::uuids::uuid ShipmentId{};
    std::optional<boost::uuids::uuid> ItemId{};
    std::optional<boost::uuids::uuid> OperationId{};
    std::optional<std::string> ContainerType{};
    std::optional<std::string> Ownership{};
    std::optional<std::string> ContainerNumber{};
    std::string Type{};
    bool Hazardous{};
};
}  // namespace transportations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transportations::models::pg::TransportationRaw> {
    static constexpr DBTypeName postgres_name = "public.transportation_v4";
};

}  // namespace userver::storages::postgres::io

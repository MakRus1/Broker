#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace shipments::models::pg {
struct PendingContainerCreation {
    boost::uuids::uuid Id{};
    std::string ContainerType{};
    std::string Ownership{};
    std::int64_t Quantity{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::PendingContainerCreation> {
    static constexpr DBTypeName postgres_name = "public.pending_container_creation_v1";
};

}  // namespace userver::storages::postgres::io

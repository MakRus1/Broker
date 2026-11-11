#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transport_locations::models::pg {
struct TransportLocationRaw {
    boost::uuids::uuid Id{};
    std::string Kind{};
    std::string Name{};
    std::string CountryCode{};
    std::optional<std::string> Code{};
};
}  // namespace transport_locations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transport_locations::models::pg::TransportLocationRaw> {
    static constexpr DBTypeName postgres_name = "public.transport_location_v1";
};

}  // namespace userver::storages::postgres::io

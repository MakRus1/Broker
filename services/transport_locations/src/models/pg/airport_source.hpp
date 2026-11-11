#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transport_locations::models::pg {
struct AirportSource {
    std::int64_t SourceId{};
    boost::uuids::uuid LocationId{};
    std::string Name{};
    std::string CountryCode{};
    std::optional<std::string> IataCode{};
    std::optional<std::string> IcaoCode{};
    std::string SearchTerms{};
};
}  // namespace transport_locations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transport_locations::models::pg::AirportSource> {
    static constexpr DBTypeName postgres_name = "public.airport_source_v1";
};

}  // namespace userver::storages::postgres::io

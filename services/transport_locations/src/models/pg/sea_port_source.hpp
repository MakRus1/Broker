#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transport_locations::models::pg {
struct SeaPortSource {
    std::string Locode{};
    boost::uuids::uuid LocationId{};
    std::string Name{};
    std::string CountryCode{};
    std::string SearchTerms{};
};
}  // namespace transport_locations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transport_locations::models::pg::SeaPortSource> {
    static constexpr DBTypeName postgres_name = "public.sea_port_source_v1";
};

}  // namespace userver::storages::postgres::io

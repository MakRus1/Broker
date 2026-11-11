#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transport_locations::models::pg {
struct RailwayStationRaw {
    boost::uuids::uuid Id{};
    std::string Name{};
    std::string CountryCode{};
    std::optional<std::string> EsrCode{};
    std::optional<std::string> UicCode{};
    std::optional<std::string> EsrSource{};
};
}  // namespace transport_locations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transport_locations::models::pg::RailwayStationRaw> {
    static constexpr DBTypeName postgres_name = "public.railway_station_v2";
};

}  // namespace userver::storages::postgres::io

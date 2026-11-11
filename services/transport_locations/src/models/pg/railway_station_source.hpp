#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transport_locations::models::pg {
struct RailwayStationSource {
    std::string SourceId{};
    boost::uuids::uuid StationId{};
    std::string Name{};
    std::string CountryCode{};
    std::optional<std::string> EsrCode{};
    std::optional<std::string> UicCode{};
    std::string SearchTerms{};
};
}  // namespace transport_locations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transport_locations::models::pg::RailwayStationSource> {
    static constexpr DBTypeName postgres_name = "public.railway_station_source_v1";
};

}  // namespace userver::storages::postgres::io

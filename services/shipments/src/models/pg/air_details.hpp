#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <models/pg/transport_location.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace shipments::models::pg {
struct AirDetails {
    std::optional<boost::uuids::uuid> CarrierId{};
    std::optional<TransportLocation> OriginAirport{};
    std::optional<TransportLocation> DestinationAirport{};
    std::optional<userver::utils::datetime::Date> Eta{};
    std::optional<userver::utils::datetime::Date> Etd{};
    std::optional<userver::utils::datetime::Date> DepartureDate{};
    std::optional<userver::utils::datetime::Date> ArrivalDate{};
    std::optional<std::string> FlightNumber{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::AirDetails> {
    static constexpr DBTypeName postgres_name = "public.air_details_v2";
};

}  // namespace userver::storages::postgres::io

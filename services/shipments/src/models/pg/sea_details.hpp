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
struct SeaDetails {
    std::optional<TransportLocation> Pol{};
    std::optional<TransportLocation> Pod{};
    std::optional<userver::utils::datetime::Date> Eta{};
    std::optional<boost::uuids::uuid> DepartureTerminalId{};
    std::optional<boost::uuids::uuid> ArrivalTerminalId{};
    std::optional<userver::utils::datetime::Date> DepartureDate{};
    std::optional<std::string> Vessel{};
    std::optional<std::string> Voyage{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::SeaDetails> {
    static constexpr DBTypeName postgres_name = "public.sea_details_v2";
};

}  // namespace userver::storages::postgres::io

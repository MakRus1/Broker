#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <models/pg/shipment_route_segment.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <vector>

namespace shipments::models::pg {
struct ShipmentRaw {
    boost::uuids::uuid Id{};
    std::int64_t Revision{};
    std::string Number{};
    boost::uuids::uuid CustomerId{};
    std::string Reference{};
    userver::utils::datetime::Date ShipmentDate{};
    std::optional<boost::uuids::uuid> RecipientId{};
    std::optional<std::string> Logist{};
    std::optional<std::string> CargoDescription{};
    std::string Status{};
    std::optional<std::string> Incoterms{};
    std::optional<std::string> CarriageTerms{};
    std::optional<std::string> Loading{};
    std::optional<std::string> DeliveryCity{};
    std::optional<std::string> DeliveryCountry{};
    std::optional<std::string> DeliveryAddress{};
    std::vector<ShipmentRouteSegment> RouteSegments{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::ShipmentRaw> {
    static constexpr DBTypeName postgres_name = "public.shipment_v4";
};

}  // namespace userver::storages::postgres::io

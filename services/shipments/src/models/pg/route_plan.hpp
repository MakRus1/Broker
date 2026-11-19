#pragma once
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace shipments::models::pg {
struct RoutePlan {
    std::optional<std::string> OriginName{};
    std::optional<std::string> OriginCountry{};
    std::optional<std::string> OriginAddress{};
    std::optional<std::string> DestinationName{};
    std::optional<std::string> DestinationCountry{};
    std::optional<std::string> DestinationAddress{};
    std::optional<userver::utils::datetime::Date> DepartureDate{};
    std::optional<userver::utils::datetime::Date> ArrivalDate{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {
template <> struct CppToUserPg<::shipments::models::pg::RoutePlan> {
    static constexpr DBTypeName postgres_name = "public.route_plan_v1";
};
}  // namespace userver::storages::postgres::io

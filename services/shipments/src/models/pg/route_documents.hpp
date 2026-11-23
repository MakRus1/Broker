#pragma once
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/date.hpp>

namespace shipments::models::pg {
struct RouteBooking {
    std::optional<std::string> Number{};
    std::optional<std::string> Status{};
    std::optional<userver::utils::datetime::Date> PlacedDate{};
    std::optional<userver::utils::datetime::Date> ConfirmedDate{};
};

struct BillOfLading {
    std::string Number{};
    std::optional<userver::utils::datetime::Date> Date{};
    std::optional<std::string> Leg{};
};

struct AirWaybill {
    std::string Number{};
    std::optional<userver::utils::datetime::Date> Date{};
};

struct RouteDocumentEvent {
    std::optional<userver::utils::datetime::Date> Date{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {
template <> struct CppToUserPg<::shipments::models::pg::RouteBooking> {
    static constexpr DBTypeName postgres_name = "public.route_booking_v1";
};

template <> struct CppToUserPg<::shipments::models::pg::BillOfLading> {
    static constexpr DBTypeName postgres_name = "public.bill_of_lading_v1";
};

template <> struct CppToUserPg<::shipments::models::pg::AirWaybill> {
    static constexpr DBTypeName postgres_name = "public.air_waybill_v1";
};

template <> struct CppToUserPg<::shipments::models::pg::RouteDocumentEvent> {
    static constexpr DBTypeName postgres_name = "public.route_document_event_v1";
};
}  // namespace userver::storages::postgres::io

#pragma once
#include <cstdint>
#include <models/pg/shipment_business_rules.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>

namespace shipments::models::pg {
struct OrganizationShipmentRules {
    ShipmentBusinessRules Rules{};
    std::int64_t Revision{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::OrganizationShipmentRules> {
    static constexpr DBTypeName postgres_name = "public.organization_shipment_rules_v1";
};

}  // namespace userver::storages::postgres::io

#pragma once
#include <string>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <vector>

namespace shipments::models::pg {
struct ShipmentBusinessRules {
    std::string NumberTemplate{};
    bool ResetNumberYearly{};
    std::vector<std::string> Statuses{};
    std::vector<std::string> RouteTypes{};
    bool PlannedContainers{};
    std::vector<std::string> ContainerTypes{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::ShipmentBusinessRules> {
    static constexpr DBTypeName postgres_name = "public.shipment_business_rules_v1";
};

}  // namespace userver::storages::postgres::io

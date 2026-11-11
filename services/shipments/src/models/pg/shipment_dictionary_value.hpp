#pragma once
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>

namespace shipments::models::pg {
struct ShipmentDictionaryValue {
    std::string Dictionary{};
    std::string Code{};
    std::string Name{};
};
}  // namespace shipments::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::shipments::models::pg::ShipmentDictionaryValue> {
    static constexpr DBTypeName postgres_name = "public.shipment_dictionary_value_v1";
};

}  // namespace userver::storages::postgres::io

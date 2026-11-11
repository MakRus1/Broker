#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transportations::models::pg {
struct ContainerFact {
    std::optional<std::string> ContainerType{};
    std::optional<std::string> Ownership{};
    std::optional<boost::uuids::uuid> OperationId{};
    std::int64_t Quantity{};
};
}  // namespace transportations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::transportations::models::pg::ContainerFact> {
    static constexpr DBTypeName postgres_name = "public.container_fact_v1";
};

}  // namespace userver::storages::postgres::io

#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace organizations::models::pg {
struct OrganizationRaw {
    boost::uuids::uuid Id{};
    std::string Name{};
    bool Enabled{};
    std::int64_t Revision{};
};
}  // namespace organizations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::organizations::models::pg::OrganizationRaw> {
    static constexpr DBTypeName postgres_name = "public.organization_v1";
};

}  // namespace userver::storages::postgres::io

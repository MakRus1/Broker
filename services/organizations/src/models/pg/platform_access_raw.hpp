#pragma once
#include <cstdint>
#include <string>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <vector>

namespace organizations::models::pg {
struct PlatformAccessRaw {
    std::vector<std::string> Permissions{};
    std::int64_t Revision{};
};
}  // namespace organizations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::organizations::models::pg::PlatformAccessRaw> {
    static constexpr DBTypeName postgres_name = "public.platform_access_v1";
};

}  // namespace userver::storages::postgres::io

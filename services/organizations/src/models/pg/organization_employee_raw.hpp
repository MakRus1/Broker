#pragma once
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>

namespace organizations::models::pg {
struct OrganizationEmployeeRaw {
    std::string Subject{};
    std::string DisplayName{};
};
}  // namespace organizations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::organizations::models::pg::OrganizationEmployeeRaw> {
    static constexpr DBTypeName postgres_name = "public.organization_employee_v1";
};

}  // namespace userver::storages::postgres::io

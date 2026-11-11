#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>

namespace organizations::models::pg {
struct MemberRaw {
    std::string Subject{};
    std::string Username{};
    std::string DisplayName{};
    std::string Email{};
    std::string Role{};
    bool Enabled{};
    std::int64_t Revision{};
    std::optional<std::string> LastName{};
    std::optional<std::string> FirstName{};
    std::optional<std::string> MiddleName{};
};
}  // namespace organizations::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::organizations::models::pg::MemberRaw> {
    static constexpr DBTypeName postgres_name = "public.member_v2";
};

}  // namespace userver::storages::postgres::io

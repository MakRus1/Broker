#pragma once
#include <components/users.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/result_set.hpp>

namespace organizations::helpers {
inline models::Organization Convert(const pgtypes::OrganizationV1& row) {
    return {.enabled = row.Enabled,
        .id = boost::uuids::to_string(row.Id),
        .name = row.Name,
        .revision = row.Revision};
}

inline models::Member Convert(const pgtypes::MemberV2& row) {
    return {.display_name = row.DisplayName,
        .email = row.Email,
        .enabled = row.Enabled,
        .first_name = row.FirstName,
        .last_name = row.LastName,
        .middle_name = row.MiddleName,
        .revision = row.Revision,
        .role = row.Role,
        .subject = row.Subject,
        .username = row.Username};
}

inline models::OrganizationRole Convert(const pgtypes::OrganizationRoleV1& row) {
    return {.description = row.Description,
        .id = row.Id,
        .name = row.Name,
        .permissions = row.Permissions,
        .revision = row.Revision};
}

inline pgtypes::MemberV2 Convert(const User& user, const std::string& role) {
    return {user.Subject, user.Username, user.DisplayName, user.Email, role, true, 1, user.LastName,
        user.FirstName, user.MiddleName};
}

template <typename Row> auto ConvertRows(const userver::storages::postgres::ResultSet& rows) {
    std::vector<decltype(Convert(std::declval<Row>()))> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<Row>())
        result.push_back(Convert(row));
    return result;
}
}  // namespace organizations::helpers

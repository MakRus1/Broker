#pragma once
#include <helpers/errors.hpp>
#include <helpers/permissions.hpp>
#include <optional>
#include <organizations/sql_queries.hpp>
#include <platform/auth/identity.hpp>
#include <postgresql/types.hpp>
#include <set>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace organizations::helpers {
namespace pg = userver::storages::postgres;

struct AccessRow {
    std::string Organization;
    std::vector<std::string> Permissions;
};

struct ProvisioningRow {
    std::string Organization;
    std::string Actor;
    userver::formats::json::Value Payload;
    std::optional<std::string> Subject;
};

inline bool IsPlatformAdmin(const platform::auth::Identity& identity) {
    return identity.Permissions.contains("platform.admin");
}

inline void RequirePlatformAdmin(const platform::auth::Identity& identity) {
    if (!IsPlatformAdmin(identity))
        throw ApiError{403, "platform_admin_required", "Нужны права администратора платформы"};
}

inline void RequireAdministrator(pg::Transaction& transaction, const std::string& organization,
    const platform::auth::Identity& identity) {
    const auto organizations = transaction.Execute(sql::kLockOrganization, organization);
    if (organizations.IsEmpty())
        throw ApiError{404, "organization_not_found", "Организация не найдена"};
    if (IsPlatformAdmin(identity))
        return;
    const auto membership = transaction.Execute(
        sql::kGetAdministratorPermissions, organization, identity.Issuer, identity.Subject);
    if (membership.IsEmpty() ||
        !ManagesOrganization(membership.AsSingleRow<std::vector<std::string>>()))
        throw ApiError{403, "organization_admin_required",
            "Нужны права управления сотрудниками и ролями организации"};
}

inline pgtypes::OrganizationRoleV1 RequireRole(
    pg::Transaction& transaction, const std::string& organization, const std::string& role) {
    const auto rows = transaction.Execute(sql::kGetRole, organization, role);
    if (rows.IsEmpty())
        throw ApiError{400, "invalid_role", "Выберите существующую роль организации"};
    return rows.AsSingleRow<pgtypes::OrganizationRoleV1>();
}

}  // namespace organizations::helpers

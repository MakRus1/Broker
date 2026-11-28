#pragma once
#include <helpers/errors.hpp>
#include <helpers/permissions.hpp>
#include <models/pg/organization_role_raw.hpp>
#include <optional>
#include <organizations/sql_queries.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace organizations::helpers {
namespace pg = userver::storages::postgres;

inline bool IsPlatformAdmin(const platform::auth::Identity& identity) {
    return identity.Permissions.contains("platform.admin");
}

inline void RequirePlatformAdmin(const platform::auth::Identity& identity) {
    if (!IsPlatformAdmin(identity))
        throw ApiError<403>{"platform_admin_required", "Нужны права администратора платформы"};
}

// Проверяет права одним запросом и без блокировки. Сценарии, которые меняют состав
// администраторов, сначала вызывают LockOrganization.
inline void RequireAdministrator(pg::Transaction& transaction,
    const boost::uuids::uuid& organization, const platform::auth::Identity& identity) {
    const auto access = transaction.Execute(
        sql::kGetAdministratorAccess, organization, identity.Issuer, identity.Subject);
    if (access.IsEmpty())
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
    if (IsPlatformAdmin(identity))
        return;
    if (!ManagesOrganization(access.AsSingleRow<std::vector<std::string>>()))
        throw ApiError<403>{"organization_admin_required",
            "Нужны права управления сотрудниками и ролями организации"};
}

// Сериализует изменения, после которых в организации может не остаться администратора.
inline void LockOrganization(pg::Transaction& transaction, const boost::uuids::uuid& organization) {
    if (transaction.Execute(sql::kLockOrganization, organization).IsEmpty())
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
}

inline models::pg::OrganizationRoleRaw RequireRole(
    pg::Transaction& transaction, const boost::uuids::uuid& organization, const std::string& role) {
    const auto rows = transaction.Execute(sql::kGetRole, organization, role);
    if (rows.IsEmpty())
        throw ApiError<400>{"invalid_role", "Выберите существующую роль организации"};
    return rows.AsSingleRow<models::pg::OrganizationRoleRaw>();
}

}  // namespace organizations::helpers

#pragma once
#include <helpers/errors.hpp>
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
    std::string Role;
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
    const auto membership =
        transaction.Execute(sql::kGetMembership, organization, identity.Issuer, identity.Subject);
    if (membership.IsEmpty())
        throw ApiError{403, "organization_access_denied", "Нет доступа к организации"};
    const auto member = membership.AsSingleRow<pgtypes::MemberV1>();
    if (!member.Enabled || member.Role != "administrator")
        throw ApiError{
            403, "organization_admin_required", "Нужны права администратора организации"};
}

inline std::set<std::string> Permissions(const std::string& role) {
    std::set<std::string> result{"shipments.read", "transportations.read", "counterparties.read"};
    if (role == "operator" || role == "administrator")
        result.insert({"shipments.write", "transportations.write", "counterparties.write"});
    if (role == "administrator")
        result.insert("organizations.manage");
    if (role != "operator" && role != "administrator" && role != "viewer")
        throw ApiError{403, "invalid_role", "Неизвестная роль сотрудника"};
    return result;
}
}  // namespace organizations::helpers

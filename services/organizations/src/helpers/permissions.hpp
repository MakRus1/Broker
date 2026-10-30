#pragma once
#include <algorithm>
#include <helpers/errors.hpp>
#include <openapi/models.hpp>
#include <organizations/sql_queries.hpp>
#include <set>
#include <string>
#include <userver/storages/postgres/transaction.hpp>
#include <vector>

namespace organizations::helpers {
enum class PermissionScope { Organization, Platform };

struct PermissionRow {
    std::string Group;
    std::string Id;
    std::string Name;
    std::vector<std::string> Requires;
};

inline std::vector<PermissionRow> LoadPermissions(
    userver::storages::postgres::Transaction& transaction,
    PermissionScope scope = PermissionScope::Organization) {
    return transaction
        .Execute(scope == PermissionScope::Platform ? sql::kListPlatformPermissions
                                                    : sql::kListPermissions)
        .AsContainer<std::vector<PermissionRow>>(userver::storages::postgres::kRowTag);
}

inline std::vector<models::Permission> PermissionCatalog(
    userver::storages::postgres::Transaction& transaction,
    PermissionScope scope = PermissionScope::Organization) {
    std::vector<models::Permission> result;
    for (const auto& row : LoadPermissions(transaction, scope))
        result.push_back(
            {.dependencies = row.Requires, .group = row.Group, .id = row.Id, .name = row.Name});
    return result;
}

inline void ValidatePermissions(userver::storages::postgres::Transaction& transaction,
    const std::vector<std::string>& permissions, const std::vector<std::string>& previous = {},
    PermissionScope scope = PermissionScope::Organization) {
    const auto catalog = LoadPermissions(transaction, scope);
    std::set<std::string> selected;
    for (const auto& code : permissions) {
        if (!selected.insert(code).second ||
            (std::none_of(
                 catalog.begin(), catalog.end(), [&](const auto& row) { return row.Id == code; }) &&
                std::find(previous.begin(), previous.end(), code) == previous.end()))
            throw ApiError<400>{
                "invalid_permission", "Неизвестное, неактивное или повторяющееся разрешение"};
    }
    for (const auto& row : catalog)
        if (selected.contains(row.Id))
            for (const auto& dependency : row.Requires)
                if (!selected.contains(dependency))
                    throw ApiError<400>{"read_permission_required",
                        "Не включено зависимое разрешение: " + dependency};
}

inline bool ManagesOrganization(const std::vector<std::string>& permissions) {
    return std::find(permissions.begin(), permissions.end(), "organizations.manage") !=
        permissions.end();
}
}  // namespace organizations::helpers

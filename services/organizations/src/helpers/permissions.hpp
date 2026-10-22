#pragma once
#include <algorithm>
#include <helpers/errors.hpp>
#include <openapi/models.hpp>
#include <set>
#include <string>
#include <vector>

namespace organizations::helpers {
inline std::vector<models::Permission> PermissionCatalog() {
    return {{.group = "Поставки", .id = "shipments.read", .name = "Просмотр поставок"},
        {.group = "Поставки", .id = "shipments.write", .name = "Изменение поставок"},
        {.group = "Перевозки", .id = "transportations.read", .name = "Просмотр перевозок"},
        {.group = "Перевозки", .id = "transportations.write", .name = "Изменение перевозок"},
        {.group = "Контрагенты", .id = "counterparties.read", .name = "Просмотр контрагентов"},
        {.group = "Контрагенты", .id = "counterparties.write", .name = "Изменение контрагентов"},
        {.group = "Администрирование организации",
            .id = "organizations.manage",
            .name = "Управление сотрудниками и ролями"}};
}

inline void ValidatePermissions(const std::vector<std::string>& permissions) {
    std::set<std::string> known;
    for (const auto& permission : PermissionCatalog())
        known.insert(permission.id);
    std::set<std::string> selected;
    for (const auto& permission : permissions) {
        if (!known.contains(permission) || !selected.insert(permission).second)
            throw ApiError{400, "invalid_permission", "Неизвестное или повторяющееся разрешение"};
    }
    for (const auto& resource : {"shipments", "transportations", "counterparties"})
        if (selected.contains(std::string(resource) + ".write") &&
            !selected.contains(std::string(resource) + ".read"))
            throw ApiError{
                400, "read_permission_required", "Для изменения данных также включите их просмотр"};
}

inline bool ManagesOrganization(const std::vector<std::string>& permissions) {
    return std::find(permissions.begin(), permissions.end(), "organizations.manage") !=
        permissions.end();
}
}  // namespace organizations::helpers

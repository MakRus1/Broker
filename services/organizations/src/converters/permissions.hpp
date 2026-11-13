#pragma once
#include <models/permission_row.hpp>
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace organizations::converters {

inline models::Permission Convert(const models::PermissionRow& row) {
    return {.dependencies = row.Requires, .group = row.Group, .id = row.Id, .name = row.Name};
}

// Имена полей документа хранения сохранены для совместимости каталогов релизов.
inline userver::formats::json::Value Convert(
    const std::vector<models::ReleasePermission>& entries) {
    namespace json = userver::formats::json;
    json::ValueBuilder rows{json::Type::kArray};
    for (const auto& entry : entries) {
        json::ValueBuilder row;
        row["code"] = entry.code;
        row["name"] = entry.name;
        row["group"] = entry.group;
        row["scope"] = entry.scope;
        row["requires"] = entry.dependencies;
        rows.PushBack(row.ExtractValue());
    }
    return rows.ExtractValue();
}
}  // namespace organizations::converters

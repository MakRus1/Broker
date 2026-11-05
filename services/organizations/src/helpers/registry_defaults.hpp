#pragma once
#include <helpers/access.hpp>
#include <openapi/models.hpp>
#include <userver/formats/json/serialize.hpp>

namespace organizations::helpers {
struct RegistryDefaultRow {
    std::string Role;
    std::string Registry;
    userver::formats::json::Value Value;
    std::int64_t Revision;
};

inline models::RegistryDefault ConvertRegistryDefault(const RegistryDefaultRow& row) {
    return {
        .registry = row.Registry, .revision = row.Revision, .role = row.Role, .value = row.Value};
}

inline std::vector<models::RegistryDefault> RegistryDefaults(const pg::ResultSet& rows) {
    std::vector<models::RegistryDefault> result;
    for (const auto& row : rows.AsSetOf<RegistryDefaultRow>(pg::kRowTag))
        result.push_back(ConvertRegistryDefault(row));
    return result;
}

inline void ValidateRegistryDefault(const userver::formats::json::Value& value) {
    if (userver::formats::json::ToString(value).size() > 24 * 1024)
        throw ApiError<400>{"registry_default_too_large", "Вид реестра не должен превышать 24 КиБ"};
    // Пустое значение снимает переопределение, сохраняя ревизию для защиты от гонок.
    if (value.IsObject() && value.GetSize() == 0)
        return;
    if (!value.HasMember("columns") || !value["columns"].IsArray() || !value.HasMember("query") ||
        !value["query"].IsObject())
        throw ApiError<400>{"invalid_registry_default", "Укажите столбцы и условия реестра"};
}
}  // namespace organizations::helpers

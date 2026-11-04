#pragma once
#include <helpers/errors.hpp>
#include <organizations/sql_queries.hpp>
#include <regex>
#include <set>
#include <userver/storages/postgres/transaction.hpp>

namespace organizations::helpers {
inline void ValidatePermissionRelease(const std::string& owner, const std::string& version) {
    static const std::regex owner_pattern{"[a-z][a-z0-9_]*"};
    if (!std::regex_match(owner, owner_pattern) || version.empty() ||
        version.find_first_of("\r\n") != std::string::npos)
        throw ApiError<400>{
            "invalid_permission_release", "Некорректный владелец или версия каталога"};
}

inline void RequirePermissionReleases(userver::storages::postgres::Transaction& tx,
    const std::string& owner, const std::vector<std::string>& versions) {
    const std::set<std::string> unique{versions.begin(), versions.end()};
    if (unique.empty() || unique.size() != versions.size())
        throw ApiError<400>{
            "invalid_permission_versions", "Укажите непустой список уникальных версий"};
    for (const auto& version : versions)
        ValidatePermissionRelease(owner, version);
    if (tx.Execute(sql::kCountPermissionReleases, owner, versions).AsSingleRow<std::int64_t>() !=
        static_cast<std::int64_t>(versions.size()))
        throw ApiError<409>{
            "permission_release_not_found", "Каталог одной из версий не зарегистрирован"};
}
}  // namespace organizations::helpers

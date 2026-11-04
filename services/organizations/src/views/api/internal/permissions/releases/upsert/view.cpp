#include <functional>
#include <helpers/permission_releases.hpp>
#include <map>
#include <openapi/models.hpp>
#include <userver/formats/json/serialize.hpp>
#include <views/api/internal/permissions/releases/upsert/view.hpp>

namespace organizations::views::api::internal::permissions::releases::upsert {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::organizations::helpers;
    namespace pg = userver::storages::postgres;
    namespace json = userver::formats::json;
    ValidatePermissionRelease(request.body.owner, request.body.version);
    static const std::regex code_pattern{"[a-z][a-z0-9_]*\\.[a-z][a-z0-9_]*"};
    std::set<std::string> codes;
    for (const auto& entry : request.body.entries) {
        if (!std::regex_match(entry.code, code_pattern) ||
            !entry.code.starts_with(request.body.owner + ".") || !codes.insert(entry.code).second ||
            entry.name.find_first_not_of(" \t\r\n") == std::string::npos ||
            entry.group.find_first_not_of(" \t\r\n") == std::string::npos)
            throw ApiError<400>{
                "invalid_permission_catalog", "Некорректное или повторяющееся право"};
        std::set<std::string> dependencies;
        for (const auto& dependency : entry.dependencies)
            if (!std::regex_match(dependency, code_pattern) || dependency == entry.code ||
                !dependencies.insert(dependency).second)
                throw ApiError<400>{
                    "invalid_permission_dependencies", "Некорректные зависимости права"};
    }
    std::map<std::string, const models::ReleasePermission*> catalog;
    for (const auto& entry : request.body.entries)
        catalog.emplace(entry.code, &entry);
    std::set<std::string> visiting;
    std::set<std::string> visited;
    const std::function<void(const std::string&)> visit = [&](const std::string& code) {
        if (visited.contains(code))
            return;
        if (!visiting.insert(code).second)
            throw ApiError<400>{"invalid_permission_dependencies", "Цикл зависимостей прав"};
        const auto& entry = *catalog.at(code);
        for (const auto& dependency : entry.dependencies) {
            const auto found = catalog.find(dependency);
            if (found == catalog.end()) {
                if (dependency.starts_with(request.body.owner + "."))
                    throw ApiError<400>{
                        "invalid_permission_dependencies", "Неизвестное право владельца"};
                continue;  // Межсервисные зависимости проверяет полный каталог релиза в CI.
            }
            if (found->second->scope != entry.scope)
                throw ApiError<400>{
                    "invalid_permission_dependencies", "Несовместимая область зависимости"};
            visit(dependency);
        }
        visiting.erase(code);
        visited.insert(code);
    };
    for (const auto& [code, entry] : catalog)
        visit(code);
    json::ValueBuilder rows{json::Type::kArray};
    for (const auto& entry : request.body.entries) {
        json::ValueBuilder row;
        row["code"] = entry.code;
        row["name"] = entry.name;
        row["group"] = entry.group;
        row["scope"] = entry.scope;
        row["requires"] = entry.dependencies;
        rows.PushBack(row.ExtractValue());
    }
    auto entries = rows.ExtractValue();
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockPermissionReleases);
    const auto existing =
        tx.Execute(sql::kGetPermissionRelease, request.body.owner, request.body.version, entries);
    if (!existing.IsEmpty() && !existing.AsSingleRow<bool>())
        throw ApiError<409>{
            "permission_release_changed", "Каталог зарегистрированной версии нельзя изменить"};
    tx.Execute(sql::kRegisterPermissionRelease, request.body.owner, request.body.version, entries);
    tx.Commit();
    return Response200{.success = true};
}
}  // namespace organizations::views::api::internal::permissions::releases::upsert

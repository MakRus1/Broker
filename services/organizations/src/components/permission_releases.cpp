#include <components/permission_releases.hpp>
#include <converters/organizations.hpp>
#include <converters/permissions.hpp>
#include <functional>
#include <helpers/access.hpp>
#include <helpers/errors.hpp>
#include <map>
#include <openapi/models.hpp>
#include <organizations/sql_queries.hpp>
#include <regex>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace organizations {
using namespace helpers;
using converters::Convert;
using converters::ConvertRows;

namespace {
void ValidatePermissionRelease(const std::string& owner, const std::string& version) {
    static const std::regex owner_pattern{"[a-z][a-z0-9_]*"};
    if (!std::regex_match(owner, owner_pattern) || version.empty() ||
        version.find_first_of("\r\n") != std::string::npos)
        throw ApiError<400>{
            "invalid_permission_release", "Некорректный владелец или версия каталога"};
}

void RequirePermissionReleases(userver::storages::postgres::Transaction& tx,
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
}  // namespace

PermissionReleasesComponent::PermissionReleasesComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema PermissionReleasesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::PermissionReleaseResult PermissionReleasesComponent::Upsert(
    const models::PermissionReleasesUpsertRequest& request,
    userver::server::request::RequestContext& context) const {
    namespace pg = userver::storages::postgres;
    ValidatePermissionRelease(request.owner, request.version);
    static const std::regex code_pattern{"[a-z][a-z0-9_]*\\.[a-z][a-z0-9_]*"};
    std::set<std::string> codes;
    for (const auto& entry : request.entries) {
        if (!std::regex_match(entry.code, code_pattern) ||
            !entry.code.starts_with(request.owner + ".") || !codes.insert(entry.code).second ||
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
    for (const auto& entry : request.entries)
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
                if (dependency.starts_with(request.owner + "."))
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
    const auto entries = converters::Convert(request.entries);
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockPermissionReleases);
    const auto existing =
        tx.Execute(sql::kGetPermissionRelease, request.owner, request.version, entries);
    if (!existing.IsEmpty() && !existing.AsSingleRow<bool>())
        throw ApiError<409>{
            "permission_release_changed", "Каталог зарегистрированной версии нельзя изменить"};
    tx.Execute(sql::kRegisterPermissionRelease, request.owner, request.version, entries);
    tx.Commit();
    return models::PermissionReleaseResult{.success = true};
}

models::PermissionReleaseResult PermissionReleasesComponent::Activate(
    const models::PermissionReleasesActivateRequest& request,
    userver::server::request::RequestContext& context) const {
    namespace pg = userver::storages::postgres;
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockPermissionReleases);
    RequirePermissionReleases(tx, request.owner, std::vector<std::string>{request.version});
    tx.Execute(sql::kActivatePermissionRelease, request.owner, request.version);
    tx.Commit();
    return models::PermissionReleaseResult{.success = true};
}

models::PermissionReleaseResult PermissionReleasesComponent::Reconcile(
    const models::PermissionReleasesReconcileRequest& request,
    userver::server::request::RequestContext& context) const {
    namespace pg = userver::storages::postgres;
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockPermissionReleases);
    RequirePermissionReleases(tx, request.owner, request.versions);
    tx.Execute(sql::kReconcilePermissionReleases, request.owner, request.versions);
    tx.Commit();
    return models::PermissionReleaseResult{.success = true};
}
}  // namespace organizations

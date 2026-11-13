#include <components/registry_defaults.hpp>
#include <converters/organizations.hpp>
#include <converters/registry_defaults.hpp>
#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <models/pg/organization_role_raw.hpp>
#include <models/registry_default_row.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
using namespace organizations;

void ValidateRegistryDefault(const userver::formats::json::Value& value) {
    if (userver::formats::json::ToString(value).size() > 24 * 1024)
        throw ApiError<400>{"registry_default_too_large", "Вид реестра не должен превышать 24 КиБ"};
    // Пустое значение снимает переопределение, сохраняя ревизию для защиты от гонок.
    if (value.IsObject() && value.GetSize() == 0)
        return;
    if (!value.HasMember("columns") || !value["columns"].IsArray() || !value.HasMember("query") ||
        !value["query"].IsObject())
        throw ApiError<400>{"invalid_registry_default", "Укажите столбцы и условия реестра"};
}
}  // namespace

namespace organizations {
using namespace helpers;

RegistryDefaultsComponent::RegistryDefaultsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema RegistryDefaultsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::RegistryDefaultsGetResult RegistryDefaultsComponent::GetOwnRegistryDefaults(
    const models::OrganizationIdentity& request, const platform::auth::Identity& identity) const {
    const auto membership = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.organization_id);
    if (membership.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};

    return models::RegistryDefaultsGetResult{
        .items = converters::ConvertRegistryDefaults(
            cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListOwnRegistryDefaults,
                request.organization_id, identity.Issuer, identity.Subject))};
}

models::RegistryDefaultsListResult RegistryDefaultsComponent::ListRoleRegistryDefaults(
    const models::OrganizationIdentity& request, const platform::auth::Identity& identity) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, request.organization_id, identity);
    models::RegistryDefaultsListResult response{
        .items = converters::ConvertRegistryDefaults(
            transaction.Execute(sql::kListRoleRegistryDefaults, request.organization_id))};
    for (const auto& role : transaction.Execute(sql::kListRoles, request.organization_id)
                                .AsSetOf<models::pg::OrganizationRoleRaw>())
        response.roles.push_back({.id = role.Id, .name = role.Name});

    transaction.Commit();
    return response;
}

models::RegistryDefaultsSaveResult RegistryDefaultsComponent::SaveRoleRegistryDefault(
    const models::RegistryDefaultsSaveRequest& request,
    const platform::auth::Identity& identity) const {
    ValidateRegistryDefault(request.value);

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, request.organization_id, identity);
    RequireRole(transaction, request.organization_id, request.role);
    const auto rows =
        transaction.Execute(request.expected_revision == 0 ? sql::kSaveRoleRegistryDefault
                                                           : sql::kUpdateRoleRegistryDefault,
            request.organization_id, request.role, request.registry, request.value,
            request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"registry_default_changed", "Вид роли уже изменён. Обновите данные"};
    const auto item =
        converters::Convert(rows.AsSingleRow<models::RegistryDefaultRow>(pg::kRowTag));
    WriteAudit(
        transaction, request.organization_id, identity, "role.registry_default.updated", item);

    transaction.Commit();
    return models::RegistryDefaultsSaveResult{.item = item};
}
}  // namespace organizations

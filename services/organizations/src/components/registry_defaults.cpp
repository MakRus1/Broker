#include <components/registry_defaults.hpp>
#include <converters/organizations.hpp>
#include <converters/registry_defaults.hpp>
#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <models/pg/organization_role_raw.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace organizations {
using namespace helpers;
using converters::Convert;
using converters::ConvertRows;

namespace {
std::vector<models::RegistryDefault> RegistryDefaults(const pg::ResultSet& rows) {
    std::vector<models::RegistryDefault> result;
    for (const auto& row : rows.AsSetOf<converters::RegistryDefaultRow>(pg::kRowTag))
        result.push_back(converters::Convert(row));
    return result;
}

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

RegistryDefaultsComponent::RegistryDefaultsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema RegistryDefaultsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::RegistryDefaultsGetResult RegistryDefaultsComponent::Get(
    const models::OrganizationIdentity& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto membership = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.organization_id);
    if (membership.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    return models::RegistryDefaultsGetResult{
        .items = RegistryDefaults(
            cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListOwnRegistryDefaults,
                request.organization_id, identity.Issuer, identity.Subject))};
}

models::RegistryDefaultsListResult RegistryDefaultsComponent::List(
    const models::OrganizationIdentity& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, request.organization_id, identity);
    models::RegistryDefaultsListResult response{
        .items = RegistryDefaults(
            transaction.Execute(sql::kListRoleRegistryDefaults, request.organization_id))};
    for (const auto& role : transaction.Execute(sql::kListRoles, request.organization_id)
                                .AsSetOf<models::pg::OrganizationRoleRaw>())
        response.roles.push_back({.id = role.Id, .name = role.Name});
    transaction.Commit();
    return response;
}

models::RegistryDefaultsSaveResult RegistryDefaultsComponent::Save(
    const models::RegistryDefaultsSaveRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& body = request;
    ValidateRegistryDefault(body.value);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, body.organization_id, identity);
    RequireRole(transaction, body.organization_id, body.role);
    const auto rows =
        transaction.Execute(body.expected_revision == 0 ? sql::kSaveRoleRegistryDefault
                                                        : sql::kUpdateRoleRegistryDefault,
            body.organization_id, body.role, body.registry, body.value, body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"registry_default_changed", "Вид роли уже изменён. Обновите данные"};
    const auto item =
        converters::Convert(rows.AsSingleRow<converters::RegistryDefaultRow>(pg::kRowTag));
    WriteAudit(transaction, body.organization_id, identity, "role.registry_default.updated", item);
    transaction.Commit();
    return models::RegistryDefaultsSaveResult{.item = item};
}
}  // namespace organizations

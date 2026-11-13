#include <components/roles.hpp>
#include <converters/organizations.hpp>
#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <models/pg/organization_role_raw.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace organizations {
using namespace helpers;
using converters::Convert;
using converters::ConvertRows;

RolesComponent::RolesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema RolesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::RolesListResult RolesComponent::ListRoles(
    const models::OrganizationIdentity& request, const platform::auth::Identity& identity) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, request.organization_id, identity);
    const auto roles = ConvertRows<models::pg::OrganizationRoleRaw>(
        transaction.Execute(sql::kListRoles, request.organization_id));
    const auto permissions = PermissionCatalog(transaction);

    transaction.Commit();
    return models::RolesListResult{.items = roles, .permissions = permissions};
}

models::OrganizationRoleResult RolesComponent::CreateRole(
    const models::RolesCreateRequest& request, const platform::auth::Identity& identity) const {
    try {
        const auto name = userver::utils::text::Trim(request.name);
        if (name.empty())
            throw ApiError<400>{"empty_role_name", "Укажите название роли"};

        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.organization_id, identity);
        ValidatePermissions(transaction, request.permissions);
        const models::pg::OrganizationRoleRaw value{
            request.id, name, request.description, request.permissions, 1};
        const auto role =
            Convert(transaction.Execute(sql::kCreateRole, request.organization_id, value)
                        .AsSingleRow<models::pg::OrganizationRoleRaw>());
        WriteAudit(transaction, request.organization_id, identity, "role.created", role);

        transaction.Commit();
        return models::OrganizationRoleResult{.role = role};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}

models::OrganizationRoleResult RolesComponent::UpdateRole(
    const models::RolesUpdateRequest& request, const platform::auth::Identity& identity) const {
    try {
        const auto name = userver::utils::text::Trim(request.name);
        if (name.empty())
            throw ApiError<400>{"empty_role_name", "Укажите название роли"};

        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.organization_id, identity);
        const auto previous = RequireRole(transaction, request.organization_id, request.id);
        ValidatePermissions(transaction, request.permissions, previous.Permissions);
        if (previous.Revision != request.expected_revision)
            throw ApiError<409>{"revision_conflict", "Роль уже изменена"};
        if (transaction.Execute(sql::kCountRoleMembers, request.organization_id, request.id)
                    .AsSingleRow<std::int64_t>() > 0 &&
            ManagesOrganization(previous.Permissions) &&
            !ManagesOrganization(request.permissions) &&
            transaction
                    .Execute(
                        sql::kCountAdministratorsExceptRole, request.organization_id, request.id)
                    .AsSingleRow<std::int64_t>() == 0)
            throw ApiError<409>{"last_administrator",
                "Нельзя убрать управление у последнего администратора организации"};
        const models::pg::OrganizationRoleRaw value{
            request.id, name, request.description, request.permissions, 1};
        const auto role =
            Convert(transaction.Execute(sql::kUpdateRole, request.organization_id, value)
                        .AsSingleRow<models::pg::OrganizationRoleRaw>());
        WriteAudit(transaction, request.organization_id, identity, "role.updated", role);

        transaction.Commit();
        return models::OrganizationRoleResult{.role = role};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}
}  // namespace organizations

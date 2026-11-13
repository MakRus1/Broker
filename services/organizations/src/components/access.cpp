#include <components/access.hpp>
#include <converters/organizations.hpp>
#include <converters/platform_access.hpp>
#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <models/access_row.hpp>
#include <models/pg/organization_employee_raw.hpp>
#include <models/pg/organization_raw.hpp>
#include <models/pg/platform_access_raw.hpp>
#include <permissions/permissions.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace pg = userver::storages::postgres;
using namespace organizations;

models::PlatformAccess ReadPlatformAccess(
    pg::Transaction& transaction, const std::string& issuer, const std::string& subject) {
    const auto rows = transaction.Execute(sql::kGetPlatformAccess, issuer, subject);
    return rows.IsEmpty() ? models::PlatformAccess{.permissions = {}, .revision = 0}
                          : converters::Convert(rows.AsSingleRow<models::pg::PlatformAccessRaw>());
}
}  // namespace

namespace organizations {
using namespace helpers;
using converters::Convert;
using converters::ConvertRows;

AccessComponent::AccessComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema AccessComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::OrganizationIdentity AccessComponent::CheckMembership(
    const models::OrganizationIdentity& request, const platform::auth::Identity& identity) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.organization_id);
    if (rows.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    const auto row = rows.AsSingleRow<models::AccessRow>(pg::kRowTag);

    return models::OrganizationIdentity{.organization_id = row.Organization};
}

models::OrganizationAccess AccessComponent::ResolveOrganizationAccess(
    const models::AccessResolveOrganizationRequest& request,
    const platform::auth::Identity& identity) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.organization_id);
    if (rows.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Выберите доступную организацию"};
    const auto [organization, granted] = rows.AsSingleRow<models::AccessRow>(pg::kRowTag);
    const std::set<std::string> permissions(granted.begin(), granted.end());
    if (!permissions.contains(request.permission))
        throw ApiError<403>{"permission_denied", "Недостаточно прав в организации"};

    return models::OrganizationAccess{
        .organization_id = organization, .permissions = {permissions.begin(), permissions.end()}};
}

models::AccessMineResult AccessComponent::FindMyOrganizations(
    const platform::auth::Identity& identity) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kMyOrganizations, identity.Issuer, identity.Subject);
    const auto managed = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kManagedOrganizations,
        identity.Issuer, identity.Subject);

    return models::AccessMineResult{
        .managed_organization_ids = managed.AsContainer<std::vector<std::string>>(),
        .organizations = ConvertRows<models::pg::OrganizationRaw>(rows),
        .platform_admin = IsPlatformAdmin(identity)};
}

models::PlatformAccessResult AccessComponent::GetPlatformAccess(
    const models::AccessGetPlatformRequest& request,
    const platform::auth::Identity& identity) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto access = ReadPlatformAccess(transaction, identity.Issuer, request.subject);
    const auto catalog = PermissionCatalog(transaction, PermissionScope::Platform);

    transaction.Commit();
    return models::PlatformAccessResult{.access = access, .catalog = catalog};
}

models::PlatformAccessResult AccessComponent::UpdatePlatformAccess(
    const models::AccessUpdatePlatformRequest& request,
    const platform::auth::Identity& identity) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto previous = ReadPlatformAccess(transaction, identity.Issuer, request.subject);
    ValidatePermissions(
        transaction, request.permissions, previous.permissions, PermissionScope::Platform);
    const auto rows = transaction.Execute(
        request.expected_revision == 0 ? sql::kSetPlatformAccess : sql::kUpdatePlatformAccess,
        identity.Issuer, request.subject, request.permissions, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Платформенные права уже изменены"};
    const auto access = Convert(rows.AsSingleRow<models::pg::PlatformAccessRaw>());
    transaction.Execute(sql::kWritePlatformAccessAudit, identity.ActorId(), identity.Issuer,
        request.subject, access.permissions, access.revision);
    const auto catalog = PermissionCatalog(transaction, PermissionScope::Platform);

    transaction.Commit();
    return models::PlatformAccessResult{.access = access, .catalog = catalog};
}

models::PlatformAccess AccessComponent::ResolvePlatformAccess(
    const models::AccessResolvePlatformRequest& request,
    const platform::auth::Identity& identity) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kResolvePlatformAccess,
        identity.Issuer, identity.Subject);
    const auto access = rows.IsEmpty() ? models::PlatformAccess{.permissions = {}, .revision = 0}
                                       : Convert(rows.AsSingleRow<models::pg::PlatformAccessRaw>());
    if (request.permission != "authenticated" &&
        std::find(access.permissions.begin(), access.permissions.end(), request.permission) ==
            access.permissions.end())
        throw ApiError<403>{"permission_denied", "Недостаточно платформенных прав"};

    return models::PlatformAccess{.permissions = access.permissions, .revision = access.revision};
}

models::AccessEmployeesResult AccessComponent::ListEmployees(
    const models::AccessEmployeesRequest& request, const platform::auth::Identity& identity) const {
    if (IsPlatformAdmin(identity) && request.organization_id.has_value()) {

        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        try {
            RequireAdministrator(transaction, request.organization_id.value(), identity);
        } catch (const ApiError<404>& error) {
            // Справочник сотрудников не раскрывает наличие недоступной организации.
            throw ApiError<403>{error.Code, "Нет доступа к сотрудникам организации"};
        }
        const auto rows = transaction.Execute(sql::kListEmployees, request.organization_id.value(),
            request.subject, request.query, request.limit);
        const auto items = ConvertRows<models::pg::OrganizationEmployeeRaw>(rows);

        transaction.Commit();
        return models::AccessEmployeesResult{.items = items};
    }

    const auto access = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.organization_id);
    if (access.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Нет доступа к сотрудникам организации"};
    const auto granted = access.AsSingleRow<models::AccessRow>(pg::kRowTag);
    const std::set<std::string> permissions(granted.Permissions.begin(), granted.Permissions.end());
    if (!permissions.contains(std::string{::permissions::ORGANIZATIONS_READ_EMPLOYEES}))
        throw ApiError<403>{
            "permission_denied", "Недостаточно прав для просмотра справочника сотрудников"};
    const auto rows = cluster_->Execute(pg::ClusterHostType::kSlave, sql::kListEmployees,
        granted.Organization, request.subject, request.query, request.limit);

    return models::AccessEmployeesResult{
        .items = ConvertRows<models::pg::OrganizationEmployeeRaw>(rows)};
}

models::OrganizationIdentity AccessComponent::CheckAdministrator(
    const models::OrganizationIdentity& request, const platform::auth::Identity& identity) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, request.organization_id, identity);

    transaction.Commit();
    return models::OrganizationIdentity{.organization_id = request.organization_id};
}
}  // namespace organizations

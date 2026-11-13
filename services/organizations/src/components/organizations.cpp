#include <components/organizations.hpp>
#include <converters/organizations.hpp>
#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <models/pg/organization_raw.hpp>
#include <registry/admin.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace organizations {
using namespace helpers;
using converters::Convert;
using converters::ConvertRows;
using namespace registry_query;

OrganizationsComponent::OrganizationsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema OrganizationsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::OrganizationsListResult OrganizationsComponent::ListOrganizations(
    const models::ListOrganizationsRequest& request) const {
    const auto scope = registry::CursorScope(request, "organizations");
    if (!registry::CursorMatches(request, scope, EnumValue(request.sort) == "id"))
        throw ApiError<400>{"invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};

    const auto query = OrganizationQuery(request);

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    transaction.Execute(sql::kRegistryPlan);
    auto rows =
        transaction.Execute(query.Statement, query.Parameters)
            .AsContainer<std::vector<registry::Row<models::pg::OrganizationRaw>>>(pg::kRowTag);

    const bool more = rows.size() > static_cast<std::size_t>(request.limit);
    if (more)
        rows.pop_back();

    std::optional<models::OrganizationListCursor> next;
    if (more)
        next = models::OrganizationListCursor{.id = boost::uuids::to_string(rows.back().Value.Id),
            .scope = scope,
            .value = rows.back().SortKey};
    const auto items = registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });

    transaction.Commit();
    return models::OrganizationsListResult{.items = items, .next_cursor = next};
}

models::OrganizationResult OrganizationsComponent::CreateOrganization(
    const models::CreateOrganizationRequest& request,
    const platform::auth::Identity& identity) const {
    try {
        const auto name = userver::utils::text::Trim(request.name);
        if (name.empty())
            throw ApiError<400>{"empty_name", "Укажите название организации"};

        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization =
            Convert(transaction.Execute(sql::kCreateOrganization, request.id, name)
                        .AsSingleRow<models::pg::OrganizationRaw>());
        transaction.Execute(sql::kCreateDefaultRoles, organization.id);
        WriteAudit(transaction, organization.id, identity, "organization.created", organization);

        transaction.Commit();
        return models::OrganizationResult{.organization = organization};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}

models::OrganizationResult OrganizationsComponent::UpdateOrganization(
    const models::UpdateOrganizationRequest& request,
    const platform::auth::Identity& identity) const {
    try {
        const auto name = userver::utils::text::Trim(request.name);
        if (name.empty())
            throw ApiError<400>{"empty_name", "Укажите название организации"};

        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto rows = transaction.Execute(sql::kLockOrganization, request.id);
        if (rows.IsEmpty())
            throw ApiError<404>{"organization_not_found", "Организация не найдена"};
        if (rows.AsSingleRow<models::pg::OrganizationRaw>().Revision != request.expected_revision)
            throw ApiError<409>{"revision_conflict", "Организация уже изменена"};
        const auto organization =
            Convert(transaction.Execute(sql::kUpdateOrganization, request.id, name, request.enabled)
                        .AsSingleRow<models::pg::OrganizationRaw>());
        WriteAudit(transaction, organization.id, identity, "organization.updated", organization);

        transaction.Commit();
        return models::OrganizationResult{.organization = organization};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}
}  // namespace organizations

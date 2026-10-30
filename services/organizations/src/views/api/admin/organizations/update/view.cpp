#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/update/view.hpp>

namespace organizations::views::api::admin::organizations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        RequirePlatformAdmin(identity);
        const auto name = userver::utils::text::Trim(request.body.name);
        if (name.empty())
            throw ApiError<400>{"empty_name", "Укажите название организации"};
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto rows = transaction.Execute(sql::kLockOrganization, request.body.id);
        if (rows.IsEmpty())
            throw ApiError<404>{"organization_not_found", "Организация не найдена"};
        if (rows.AsSingleRow<pgtypes::OrganizationV1>().Revision != request.body.expected_revision)
            throw ApiError<409>{"revision_conflict", "Организация уже изменена"};
        const auto organization = Convert(
            transaction
                .Execute(sql::kUpdateOrganization, request.body.id, name, request.body.enabled)
                .AsSingleRow<pgtypes::OrganizationV1>());
        WriteAudit(transaction, organization.id, identity, "organization.updated", organization);
        transaction.Commit();
        return Response200{.organization = organization};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::admin::organizations::update

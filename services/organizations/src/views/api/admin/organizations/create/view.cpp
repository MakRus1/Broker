#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/create/view.hpp>

namespace organizations::views::api::admin::organizations::create {
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
        const auto organization =
            Convert(transaction.Execute(sql::kCreateOrganization, request.body.id, name)
                        .AsSingleRow<pgtypes::OrganizationV1>());
        transaction.Execute(sql::kCreateDefaultRoles, organization.id);
        WriteAudit(transaction, organization.id, identity, "organization.created", organization);
        transaction.Commit();
        return Response201{.organization = organization};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::admin::organizations::create

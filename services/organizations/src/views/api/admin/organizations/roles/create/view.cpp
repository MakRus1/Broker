#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/roles/create/view.hpp>

namespace organizations::views::api::admin::organizations::roles::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        const auto name = userver::utils::text::Trim(request.body.name);
        if (name.empty())
            throw ApiError<400>{"empty_role_name", "Укажите название роли"};
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.body.organization_id, identity);
        ValidatePermissions(transaction, request.body.permissions);
        const pgtypes::OrganizationRoleV1 value{
            request.body.id, name, request.body.description, request.body.permissions, 1};
        const auto role =
            Convert(transaction.Execute(sql::kCreateRole, request.body.organization_id, value)
                        .AsSingleRow<pgtypes::OrganizationRoleV1>());
        WriteAudit(transaction, request.body.organization_id, identity, "role.created", role);
        transaction.Commit();
        return Response201{.role = role};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::admin::organizations::roles::create

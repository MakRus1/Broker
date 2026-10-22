#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/roles/update/view.hpp>

namespace organizations::views::api::admin::organizations::roles::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        const auto name = userver::utils::text::Trim(request.body.name);
        if (name.empty())
            throw ApiError{400, "empty_role_name", "Укажите название роли"};
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.body.organization_id, identity);
        const auto previous =
            RequireRole(transaction, request.body.organization_id, request.body.id);
        ValidatePermissions(transaction, request.body.permissions, previous.Permissions);
        if (previous.Revision != request.body.expected_revision)
            throw ApiError{409, "revision_conflict", "Роль уже изменена"};
        if (transaction
                    .Execute(sql::kCountRoleMembers, request.body.organization_id, request.body.id)
                    .AsSingleRow<std::int64_t>() > 0 &&
            ManagesOrganization(previous.Permissions) &&
            !ManagesOrganization(request.body.permissions) &&
            transaction
                    .Execute(sql::kCountAdministratorsExceptRole, request.body.organization_id,
                        request.body.id)
                    .AsSingleRow<std::int64_t>() == 0)
            throw ApiError{409, "last_administrator",
                "Нельзя убрать управление у последнего администратора организации"};
        const pgtypes::OrganizationRoleV1 value{
            request.body.id, name, request.body.description, request.body.permissions, 1};
        const auto role =
            Convert(transaction.Execute(sql::kUpdateRole, request.body.organization_id, value)
                        .AsSingleRow<pgtypes::OrganizationRoleV1>());
        WriteAudit(transaction, request.body.organization_id, identity, "role.updated", role);
        transaction.Commit();
        return Response200{.role = role};
    } catch (const ::organizations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            return Response503{.error = error.Code, .message = error.what()};
        }
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::admin::organizations::roles::update

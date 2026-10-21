#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/members/update/view.hpp>

namespace organizations::views::api::admin::organizations::members::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.body.organization_id, identity);
        const auto rows = transaction.Execute(sql::kGetMembership, request.body.organization_id,
            identity.Issuer, request.body.subject);
        if (rows.IsEmpty())
            throw ApiError{404, "member_not_found", "Сотрудник не найден"};
        const auto previous = rows.AsSingleRow<pgtypes::MemberV1>();
        if (previous.Revision != request.body.expected_revision)
            throw ApiError{409, "revision_conflict", "Доступ сотрудника уже изменён"};
        const auto role = request.body.role;
        const auto previous_role =
            RequireRole(transaction, request.body.organization_id, previous.Role);
        const auto next_role = RequireRole(transaction, request.body.organization_id, role);
        if (previous.Enabled && ManagesOrganization(previous_role.Permissions) &&
            (!request.body.enabled || !ManagesOrganization(next_role.Permissions)) &&
            transaction.Execute(sql::kCountAdministrators, request.body.organization_id)
                    .AsSingleRow<std::int64_t>() <= 1)
            throw ApiError{409, "last_administrator",
                "Нельзя отключить или понизить последнего администратора"};
        const auto member =
            Convert(transaction
                        .Execute(sql::kUpdateMember, request.body.organization_id, identity.Issuer,
                            request.body.subject, role, request.body.enabled)
                        .AsSingleRow<pgtypes::MemberV1>());
        WriteAudit(transaction, request.body.organization_id, identity, "member.updated", member);
        transaction.Commit();
        return Response200{.member = member};
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
}  // namespace organizations::views::api::admin::organizations::members::update

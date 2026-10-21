#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/members/add/view.hpp>

namespace organizations::views::api::admin::organizations::members::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        {
            auto transaction =
                deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            RequireAdministrator(transaction, request.body.organization_id, identity);
            transaction.Commit();
        }
        // Сетевое обращение выполняется без открытой транзакции и блокировок БД.
        const auto user = deps.users->Find(request.body.username);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.body.organization_id, identity);
        const auto member =
            Convert(transaction
                        .Execute(sql::kAddMember, request.body.organization_id, identity.Issuer,
                            Convert(user, models::EnumValue(request.body.role)))
                        .AsSingleRow<pgtypes::MemberV1>());
        WriteAudit(transaction, request.body.organization_id, identity, "member.added", member);
        transaction.Commit();
        return Response201{.member = member};
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
}  // namespace organizations::views::api::admin::organizations::members::add

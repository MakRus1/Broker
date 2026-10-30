#include <helpers/platform_access.hpp>
#include <views/api/admin/platform_access/update/view.hpp>

namespace organizations::views::api::admin::platform_access::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);

    RequirePlatformAdmin(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto previous = ReadPlatformAccess(transaction, identity.Issuer, request.body.subject);
    ValidatePermissions(
        transaction, request.body.permissions, previous.permissions, PermissionScope::Platform);
    const auto rows = transaction.Execute(
        request.body.expected_revision == 0 ? sql::kSetPlatformAccess : sql::kUpdatePlatformAccess,
        identity.Issuer, request.body.subject, request.body.permissions,
        request.body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Платформенные права уже изменены"};
    const auto access = Convert(rows.AsSingleRow<pgtypes::PlatformAccessV1>());
    transaction.Execute(sql::kWritePlatformAccessAudit, identity.ActorId(), identity.Issuer,
        request.body.subject, access.permissions, access.revision);
    const auto catalog = PermissionCatalog(transaction, PermissionScope::Platform);
    transaction.Commit();
    return Response200{.access = access, .catalog = catalog};
}
}  // namespace organizations::views::api::admin::platform_access::update

#include <helpers/platform_access.hpp>
#include <views/api/admin/platform_access/get/view.hpp>

namespace organizations::views::api::admin::platform_access::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        RequirePlatformAdmin(identity);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto access = ReadPlatformAccess(transaction, identity.Issuer, request.body.subject);
        const auto catalog = PermissionCatalog(transaction, PermissionScope::Platform);
        transaction.Commit();
        return Response200{.access = access, .catalog = catalog};
    } catch (const ::organizations::ApiError& error) {
        if (error.Status == 403)
            return Response403{.error = error.Code, .message = error.what()};
        if (error.Status == 409)
            return Response409{.error = error.Code, .message = error.what()};
        return Response400{.error = error.Code, .message = error.what()};
    }
}
}  // namespace organizations::views::api::admin::platform_access::get

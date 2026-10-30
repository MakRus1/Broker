#include <helpers/platform_access.hpp>
#include <views/api/admin/platform_access/get/view.hpp>

namespace organizations::views::api::admin::platform_access::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);

    RequirePlatformAdmin(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto access = ReadPlatformAccess(transaction, identity.Issuer, request.body.subject);
    const auto catalog = PermissionCatalog(transaction, PermissionScope::Platform);
    transaction.Commit();
    return Response200{.access = access, .catalog = catalog};
}
}  // namespace organizations::views::api::admin::platform_access::get

#include <helpers/platform_access.hpp>
#include <views/api/platform/access/view.hpp>

namespace organizations::views::api::platform::access {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = ::platform::auth::GetIdentity(context);
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kResolvePlatformAccess, identity.Issuer, identity.Subject);
    const auto access = rows.IsEmpty() ? models::PlatformAccess{.permissions = {}, .revision = 0}
                                       : Convert(rows.AsSingleRow<pgtypes::PlatformAccessV1>());
    if (request.body.permission != "authenticated" &&
        std::find(access.permissions.begin(), access.permissions.end(), request.body.permission) ==
            access.permissions.end())
        return Response403{
            .error = "permission_denied", .message = "Недостаточно платформенных прав"};
    return Response200{.permissions = access.permissions, .revision = access.revision};
}
}  // namespace organizations::views::api::platform::access

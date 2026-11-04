#include <helpers/permission_releases.hpp>
#include <views/api/internal/permissions/releases/activate/view.hpp>

namespace organizations::views::api::internal::permissions::releases::activate {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::organizations::helpers;
    namespace pg = userver::storages::postgres;
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockPermissionReleases);
    RequirePermissionReleases(
        tx, request.body.owner, std::vector<std::string>{request.body.version});
    tx.Execute(sql::kActivatePermissionRelease, request.body.owner, request.body.version);
    tx.Commit();
    return Response200{.success = true};
}
}  // namespace organizations::views::api::internal::permissions::releases::activate

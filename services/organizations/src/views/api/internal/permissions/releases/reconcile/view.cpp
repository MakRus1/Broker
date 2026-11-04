#include <helpers/permission_releases.hpp>
#include <views/api/internal/permissions/releases/reconcile/view.hpp>

namespace organizations::views::api::internal::permissions::releases::reconcile {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::organizations::helpers;
    namespace pg = userver::storages::postgres;
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockPermissionReleases);
    RequirePermissionReleases(tx, request.body.owner, request.body.versions);
    tx.Execute(sql::kReconcilePermissionReleases, request.body.owner, request.body.versions);
    tx.Commit();
    return Response200{.success = true};
}
}  // namespace organizations::views::api::internal::permissions::releases::reconcile

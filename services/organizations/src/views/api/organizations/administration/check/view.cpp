#include <helpers/access.hpp>
#include <views/api/organizations/administration/check/view.hpp>

namespace organizations::views::api::organizations::administration::check {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;

    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(
        transaction, request.body.organization_id, platform::auth::GetIdentity(context));
    transaction.Commit();
    return Response200{.organization_id = request.body.organization_id};
}
}  // namespace organizations::views::api::organizations::administration::check

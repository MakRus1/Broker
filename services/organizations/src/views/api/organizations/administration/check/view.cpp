#include <helpers/access.hpp>
#include <views/api/organizations/administration/check/view.hpp>

namespace organizations::views::api::organizations::administration::check {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    try {
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(
            transaction, request.body.organization_id, platform::auth::GetIdentity(context));
        transaction.Commit();
        return Response200{.organization_id = request.body.organization_id};
    } catch (const ::organizations::ApiError& error) {
        if (error.Status == 404)
            return Response404{.error = error.Code, .message = error.what()};
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace organizations::views::api::organizations::administration::check

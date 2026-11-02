#include <helpers/preferences.hpp>
#include <views/api/v1/preferences/list/view.hpp>

namespace user_preferences::views::api::v1::preferences::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::user_preferences::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    CheckOrganization(deps, context, request.body.organization_id);
    return Response200{.preferences = Convert(deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
                           sql::kListPreferences, request.body.organization_id, identity.Issuer,
                           identity.Subject))};
}
}  // namespace user_preferences::views::api::v1::preferences::list

#include <helpers/access.hpp>
#include <views/api/organizations/membership/get/view.hpp>

namespace organizations::views::api::organizations::membership::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.body.organization_id);
    if (rows.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    const auto row = rows.AsSingleRow<AccessRow>(pg::kRowTag);
    return Response200{.organization_id = row.Organization};
}
}  // namespace organizations::views::api::organizations::membership::get

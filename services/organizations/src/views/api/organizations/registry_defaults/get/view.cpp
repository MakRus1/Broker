#include <helpers/registry_defaults.hpp>
#include <views/api/organizations/registry_defaults/get/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto membership = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kResolveAccess, identity.Issuer, identity.Subject, request.body.organization_id);
    if (membership.Size() != 1)
        throw ApiError<403>{"organization_access_denied", "Нет доступа к организации"};
    return Response200{.items = RegistryDefaults(deps.pg_cluster->Execute(
                           pg::ClusterHostType::kMaster, sql::kListOwnRegistryDefaults,
                           request.body.organization_id, identity.Issuer, identity.Subject))};
}
}  // namespace organizations::views::api::organizations::registry_defaults::get

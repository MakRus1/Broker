#include <helpers/registry_defaults.hpp>
#include <views/api/organizations/registry_defaults/list/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, request.body.organization_id, identity);
    Response200 response{.items = RegistryDefaults(transaction.Execute(
                             sql::kListRoleRegistryDefaults, request.body.organization_id))};
    for (const auto& role : transaction.Execute(sql::kListRoles, request.body.organization_id)
                                .AsSetOf<pgtypes::OrganizationRoleV1>())
        response.roles.push_back({.id = role.Id, .name = role.Name});
    transaction.Commit();
    return response;
}
}  // namespace organizations::views::api::organizations::registry_defaults::list

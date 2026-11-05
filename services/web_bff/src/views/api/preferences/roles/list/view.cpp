#include <platform/browser/session.hpp>
#include <views/api/preferences/roles/list/view.hpp>
namespace web_bff::views::api::preferences::roles::list {
Response View::Handle(Request&& request, userver::server::request::RequestContext& context,
                      Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiOrganizationsRegistryDefaultsList;
    const auto result = deps.organizations.ApiOrganizationsRegistryDefaultsList(
        api::Request{.body = {.organization_id = request.body.organization_id}}, session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back({.registry = item.registry,
                                  .revision = item.revision,
                                  .role = item.role,
                                  .value = item.value});
    for (const auto& role : result.roles)
        response.roles.push_back({.id = role.id, .name = role.name});
    return response;
}
}  // namespace web_bff::views::api::preferences::roles::list

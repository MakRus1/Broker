#include <platform/browser/session.hpp>
#include <views/api/preferences/list/view.hpp>

namespace web_bff::views::api::preferences::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::user_preferences::ApiV1PreferencesList;
    const auto result = deps.user_preferences.ApiV1PreferencesList(
        api::Request{.body = {.organization_id = request.body.organization_id}}, session.Access);
    Response200 response;
    for (const auto& preference : result.preferences)
        response.preferences.push_back(
            {.key = preference.key, .revision = preference.revision, .value = preference.value});
    namespace defaults = clients::organizations::ApiOrganizationsRegistryDefaultsGet;
    const auto role = deps.organizations.ApiOrganizationsRegistryDefaultsGet(
        defaults::Request{.body = {.organization_id = request.body.organization_id}},
        session.Access);
    for (const auto& item : role.items)
        response.role_defaults.push_back({.registry = item.registry,
            .revision = item.revision,
            .role = item.role,
            .value = item.value});
    return response;
}
}  // namespace web_bff::views::api::preferences::list

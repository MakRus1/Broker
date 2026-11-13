#include <components/preferences.hpp>
#include <platform/browser/session.hpp>
#include <utility>
#include <views/api/preferences/roles/list/view.hpp>

namespace web_bff::views::api::preferences::roles::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    auto result =
        deps.preferences.ListRoleRegistryDefaults(request.body.organization_id, session.Access);
    return Response200{.items = std::move(result.items), .roles = std::move(result.roles)};
}
}  // namespace web_bff::views::api::preferences::roles::list

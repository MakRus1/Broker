#include <components/preferences.hpp>
#include <platform/browser/session.hpp>
#include <utility>
#include <views/api/preferences/list/view.hpp>

namespace web_bff::views::api::preferences::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    auto result =
        deps.preferences.GetWorkspacePreferences(request.body.organization_id, session.Access);
    return Response200{.preferences = std::move(result.preferences),
        .role_defaults = std::move(result.role_defaults)};
}
}  // namespace web_bff::views::api::preferences::list

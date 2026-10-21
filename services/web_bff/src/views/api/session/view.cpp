#include <platform/browser/session.hpp>
#include <views/api/session/view.hpp>

namespace web_bff::views::api::session {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::organizations::ApiOrganizationsAccess;
    const auto access = deps.organizations.ApiOrganizationsAccess(
        api::Request{.body = api::RequestBody{.permission = "shipments.read"}}, session.Access);
    return Response200{.actor = session.User.ActorId(),
        .csrf_token = session.Csrf,
        .display_name = session.User.DisplayName,
        .permissions = access.permissions,
        .subject = session.User.Subject};
}
}  // namespace web_bff::views::api::session

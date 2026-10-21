#include <platform/browser/session.hpp>
#include <views/api/session/view.hpp>

namespace admin_bff::views::api::session {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    return Response200{.actor = session.User.ActorId(),
        .csrf_token = session.Csrf,
        .display_name = session.User.DisplayName,
        .permissions = {session.User.Permissions.begin(), session.User.Permissions.end()},
        .subject = session.User.Subject};
}
}  // namespace admin_bff::views::api::session

#include <platform/browser/session.hpp>
#include <views/api/session/view.hpp>

namespace admin_bff::views::api::session {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.admin_session.GetAdminSession(session);
    return Response200{.actor = result.actor,
        .csrf_token = result.csrf_token,
        .display_name = result.display_name,
        .permissions = result.permissions,
        .subject = result.subject};
}
}  // namespace admin_bff::views::api::session

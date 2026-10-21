#include <platform/browser/session.hpp>
#include <views/api/logout/view.hpp>

namespace admin_bff::views::api::logout {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    deps.sessions->Logout(session.Id);
    session.Request->GetHttpResponse().SetCookie(
        deps.sessions->Cookie(deps.sessions->SessionCookie, "", 0));
    return Response200{.logout_url = deps.sessions->LogoutUrl()};
}
}  // namespace admin_bff::views::api::logout

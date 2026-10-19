#include <transport/login.hpp>
#include <transport/responses.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_response.hpp>

namespace admin_bff {
using Http = userver::server::http::HttpRequest;

Login::Login(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), sessions_(context.FindComponent<Sessions>()) {}

std::string Login::HandleRequestThrow(
    const Http& http, userver::server::request::RequestContext&) const {
    const auto [state, url] = sessions_.Begin();
    http.GetHttpResponse().SetCookie(sessions_.Cookie("broker_login", state, 300));
    return detail::Redirect(http, url);
}

LoginCallback::LoginCallback(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), sessions_(context.FindComponent<Sessions>()) {}

std::string LoginCallback::HandleRequestThrow(
    const Http& http, userver::server::request::RequestContext&) const {
    auto& response = http.GetHttpResponse();
    response.SetCookie(sessions_.Cookie("broker_login", "", 0));
    const auto state = http.GetArg("state");
    if (state.empty() || state != http.GetCookie(sessions_.CookieName("broker_login")) ||
        http.GetArgVector("state").size() != 1 || http.GetArgVector("code").size() != 1 ||
        http.GetArg("code").size() > 8192)
        return detail::JsonError(
            http, 400, "invalid_login", "Вход не завершён. Начните вход заново.");
    const auto id = sessions_.Complete(state, http.GetArg("code"));
    response.SetCookie(sessions_.Cookie("broker_admin", id, 28800));
    return detail::Redirect(http, "/");
}
}  // namespace admin_bff

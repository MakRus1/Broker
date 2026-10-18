#include <transport/admin.hpp>
#include <transport/responses.hpp>
#include <userver/server/http/http_response.hpp>
namespace admin_bff {
using Http = userver::server::http::HttpRequest;
using detail::JsonError;
using detail::Redirect;
std::string Admin::HandleLogin(const Http& http) const {
    auto& response = http.GetHttpResponse();
    const auto path = http.GetRequestPath();
    if (path == "/auth/login") {
        const auto [state, url] = sessions_.Begin();
        response.SetCookie(sessions_.Cookie("broker_login", state, 300));
        return Redirect(http, url);
    }
    if (path == "/auth/callback") {
        response.SetCookie(sessions_.Cookie("broker_login", "", 0));
        const auto state = http.GetArg("state");
        if (state.empty() || state != http.GetCookie(sessions_.CookieName("broker_login")) ||
            http.GetArgVector("state").size() != 1 || http.GetArgVector("code").size() != 1 ||
            http.GetArg("code").size() > 8192)
            return JsonError(http, 400, "invalid_login", "Вход не завершён. Начните вход заново.");
        const auto id = sessions_.Complete(state, http.GetArg("code"));
        response.SetCookie(sessions_.Cookie("broker_admin", id, 28800));
        return Redirect(http, "/");
    }
    return JsonError(http, 404, "not_found", "Метод не найден");
}
}  // namespace admin_bff

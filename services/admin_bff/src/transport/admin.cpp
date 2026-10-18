#include <transport/admin.hpp>
#include <transport/responses.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/fs_cache.hpp>
#include <userver/formats/json.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/http/http_response.hpp>

namespace {
using Http = userver::server::http::HttpRequest;
void CheckCsrf(const Http& http, const admin_bff::Sessions& sessions,
               const admin_bff::Session& session) {
    if (http.GetHeader("Origin") != sessions.Origin ||
        http.GetHeader("X-CSRF-Token") != session.Csrf)
        throw platform::auth::Forbidden{};
}
}  // namespace
namespace admin_bff {
using detail::JsonError;
Admin::Admin(const userver::components::ComponentConfig& config,
             const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), sessions_(context.FindComponent<Sessions>()),
      assets_(context.FindComponent<userver::components::FsCache>("admin-assets").GetClient()),
      deps_(ResolveDeps(context)) {}
std::string Admin::HandleRequestThrow(const Http& http,
                                      userver::server::request::RequestContext&) const {
    auto& response = http.GetHttpResponse();
    response.SetHeader(std::string_view{"Content-Type"}, "application/json");
    response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
    response.SetHeader(std::string_view{"X-Content-Type-Options"}, "nosniff");
    response.SetHeader(std::string_view{"Referrer-Policy"}, "no-referrer");
    response.SetHeader(
        std::string_view{"Content-Security-Policy"},
        "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src "
        "'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    const auto path = http.GetRequestPath();
    const bool get = http.GetMethodStr() == "GET";
    try {
        if (path.starts_with("/auth/") && get)
            return HandleLogin(http);
        if (!path.starts_with("/api/") && get)
            return ServeAssets(http);
        const auto session = sessions_.Get(http.GetCookie(sessions_.CookieName("broker_admin")));
        if (path == "/api/logout" && !get) {
            {
                std::lock_guard lock(session->Mutex);
                CheckCsrf(http, sessions_, *session);
            }
            sessions_.Logout(http.GetCookie(sessions_.CookieName("broker_admin")));
            response.SetCookie(sessions_.Cookie("broker_admin", "", 0));
            userver::formats::json::ValueBuilder body;
            body["logout_url"] = sessions_.LogoutUrl();
            return userver::formats::json::ToString(body.ExtractValue());
        }
        std::lock_guard lock(session->Mutex);
        sessions_.EnsureFresh(*session);
        if (path == "/api/session" && get) {
            userver::formats::json::ValueBuilder body;
            body["actor"] = session->User.ActorId();
            body["subject"] = session->User.Subject;
            body["display_name"] = session->User.DisplayName;
            body["csrf_token"] = session->Csrf;
            body["permissions"] =
                userver::formats::json::ValueBuilder(userver::formats::json::Type::kArray);
            for (const auto& permission : session->User.Permissions)
                body["permissions"].PushBack(permission);
            return userver::formats::json::ToString(body.ExtractValue());
        }
        if (!get) {
            CheckCsrf(http, sessions_, *session);
            if (http.GetHeader("Content-Type") != "application/json")
                return JsonError(http, 400, "invalid_request", "Ожидается JSON");
        }
        const auto body = get ? userver::formats::json::Value{}
                              : userver::formats::json::FromString(http.RequestBody());
        return Forward(path, body, session->Access, http);
    } catch (const platform::http::ApiError& error) {
        http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>(error.GetStatus()));
        return error.GetBody();
    } catch (const platform::auth::Forbidden&) {
        return JsonError(http, 403, "forbidden", "Запрос отклонён: проверьте права и CSRF-токен");
    } catch (const userver::server::handlers::Unauthorized&) {
        response.SetCookie(sessions_.Cookie("broker_admin", "", 0));
        return JsonError(http, 401, "unauthorized", "Войдите в систему");
    } catch (const CapacityExceeded&) {
        return JsonError(http, 429, "too_many_sessions",
                         "Слишком много попыток входа. Повторите позже.");
    } catch (const platform::auth::Unavailable&) {
        return JsonError(http, 503, "auth_unavailable", "Сервис входа временно недоступен");
    } catch (const platform::http::TransportError&) {
        return JsonError(http, 503, "upstream_unavailable", "Configs временно недоступен");
    } catch (const platform::http::ContractError&) {
        return JsonError(http, 502, "invalid_upstream", "Некорректный ответ Configs");
    } catch (const userver::formats::json::Exception&) {
        return JsonError(http, 400, "invalid_request", "Некорректный JSON-запрос");
    } catch (const std::invalid_argument&) {
        return JsonError(http, 400, "invalid_request", "Некорректный запрос или данные входа");
    }
}

}  // namespace admin_bff

#include <platform/browser/responses.hpp>
#include <platform/browser/session.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/handlers/exceptions.hpp>

namespace {
using Http = userver::server::http::HttpRequest;

void Error(const Http& http, int status, const std::string& error, const std::string& message) {
    http.GetHttpResponse().SetHeader(std::string_view{"Content-Type"}, "application/json");
    http.GetHttpResponse().SetData(
        platform::browser::detail::JsonError(http, status, error, message));
}

class BrowserSessionMiddleware final : public userver::server::middlewares::HttpMiddlewareBase {
  public:
    explicit BrowserSessionMiddleware(platform::browser::Sessions& sessions)
        : sessions_(sessions) {}

  private:
    void Authenticate(const Http& http, userver::server::request::RequestContext& context) const {
        const auto id = http.GetCookie(sessions_.CookieName(sessions_.SessionCookie));
        const auto session = sessions_.Get(id);
        std::lock_guard lock(session->Mutex);
        sessions_.EnsureFresh(*session);
        if (http.GetMethodStr() == "POST") {
            if (http.GetHeader("Origin") != sessions_.Origin ||
                http.GetHeader("X-CSRF-Token") != session->Csrf)
                throw platform::auth::Forbidden{};
            const auto& type = http.GetHeader("Content-Type");
            if (type != "application/json" && !type.starts_with("application/json;"))
                throw std::invalid_argument("JSON content type is required");
        }
        context.SetData("platform-auth", session->User);
        context.SetData("browser-session",
            platform::browser::BrowserSession{
                session->User, session->Access, session->Csrf, id, &http});
        // Снимок изолирован от обновления токенов; сеть и logout выполняются без этого mutex.
    }

    void HandleRequest(userver::server::http::HttpRequest& http,
        userver::server::request::RequestContext& context) const override {
        auto& response = http.GetHttpResponse();
        if (http.GetRequestPath().starts_with("/api/"))
            response.SetHeader(std::string_view{"Content-Type"}, "application/json");
        response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
        response.SetHeader(std::string_view{"X-Content-Type-Options"}, "nosniff");
        response.SetHeader(std::string_view{"Referrer-Policy"}, "no-referrer");
        const auto path = http.GetRequestPath();
        if (path != "/" && path != "/index.html" && !path.starts_with("/assets/"))
            response.SetHeader(std::string_view{"Content-Security-Policy"},
                platform::browser::detail::ContentSecurityPolicy());
        try {
            if (http.GetRequestPath().starts_with("/api/"))
                Authenticate(http, context);
            Next(http, context);
        } catch (const platform::http::ApiError& error) {
            response.SetStatus(static_cast<userver::server::http::HttpStatus>(error.GetStatus()));
            response.SetData(error.GetBody());
        } catch (const platform::auth::Forbidden&) {
            Error(http, 403, "forbidden", "Запрос отклонён: проверьте права и CSRF-токен");
        } catch (const userver::server::handlers::Unauthorized&) {
            response.SetCookie(sessions_.Cookie(sessions_.SessionCookie, "", 0));
            Error(http, 401, "unauthorized", "Войдите в систему");
        } catch (const platform::browser::CapacityExceeded&) {
            Error(http, 429, "too_many_sessions", "Слишком много попыток входа. Повторите позже.");
        } catch (const platform::auth::Unavailable&) {
            Error(http, 503, "auth_unavailable", "Сервис входа временно недоступен");
        } catch (const platform::http::TransportError&) {
            Error(http, 503, "upstream_unavailable", "Сервис данных временно недоступен");
        } catch (const platform::http::ContractError&) {
            Error(http, 502, "invalid_upstream", "Некорректный ответ сервиса данных");
        } catch (const userver::formats::json::Exception&) {
            Error(http, 400, "invalid_request", "Некорректный JSON-запрос");
        } catch (const std::invalid_argument&) {
            Error(http, 400, "invalid_request", "Некорректный запрос или данные входа");
        }
    }

    platform::browser::Sessions& sessions_;
};
}  // namespace

namespace platform::browser {
const BrowserSession& GetBrowserSession(const userver::server::request::RequestContext& context) {
    return context.GetData<BrowserSession>("browser-session");
}

BrowserSessionFactory::BrowserSessionFactory(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpMiddlewareFactoryBase(config, context), sessions_(context.FindComponent<Sessions>()) {}

std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> BrowserSessionFactory::Create(
    const userver::server::handlers::HttpHandlerBase&, userver::yaml_config::YamlConfig) const {
    return std::make_unique<BrowserSessionMiddleware>(sessions_);
}
}  // namespace platform::browser

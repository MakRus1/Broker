#pragma once
#include <platform/browser/sessions.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace platform::browser {
class Login final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "browser-login";
    Login(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override;

  private:
    Sessions& sessions_;
};

class LoginCallback final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "browser-login-callback";
    LoginCallback(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override;

    std::string GetUrlForLogging(const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const override {
        return request.GetRequestPath();
    }

  private:
    Sessions& sessions_;
};
}  // namespace platform::browser

#pragma once
#include <components/sessions.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace admin_bff {
class Login final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "admin-login";
    Login(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override;

  private:
    Sessions& sessions_;
};

class LoginCallback final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "admin-login-callback";
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
}  // namespace admin_bff

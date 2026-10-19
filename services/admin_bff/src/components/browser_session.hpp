#pragma once

#include <components/sessions.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/middlewares/http_middleware_base.hpp>
#include <userver/server/request/request_context.hpp>

namespace admin_bff {
struct BrowserSession {
    platform::auth::Principal User;
    std::string Access, Csrf, Id;
    const userver::server::http::HttpRequest* Request;
};

const BrowserSession& GetBrowserSession(const userver::server::request::RequestContext& context);

class BrowserSessionFactory final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
  public:
    static constexpr std::string_view kName = "admin-browser-session";
    BrowserSessionFactory(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

  private:
    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Create(
        const userver::server::handlers::HttpHandlerBase&,
        userver::yaml_config::YamlConfig) const override;
    Sessions& sessions_;
};
}  // namespace admin_bff

#pragma once
#include <components/sessions.hpp>
#include <deps.hpp>
#include <userver/fs/fs_cache_client.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace admin_bff {
class Admin final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "admin-transport";
    Admin(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override;

    std::string GetUrlForLogging(const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext&) const override {
        return request.GetRequestPath();
    }

  private:
    std::string Forward(const std::string& path, const userver::formats::json::Value& body,
        const std::string& token, const userver::server::http::HttpRequest& http) const;
    std::string HandleLogin(const userver::server::http::HttpRequest&) const;
    std::string ServeAssets(const userver::server::http::HttpRequest&) const;
    Sessions& sessions_;
    const userver::fs::FsCacheClient& assets_;
    Deps deps_;
};
}  // namespace admin_bff

#pragma once
#include <deps.hpp>
#include <userver/components/state.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace configs {
class Ping final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "handler-ping";
    using HttpHandlerBase::HttpHandlerBase;
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override;
};

class PingReady final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "handler-ping-ready";
    PingReady(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override;

  private:
    userver::components::State state_;
    Deps deps_;
};
}  // namespace configs

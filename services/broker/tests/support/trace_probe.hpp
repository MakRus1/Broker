#pragma once
#include <atomic>
#include <memory>
#include <platform/readiness.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/ugrpc/client/client_factory.hpp>
#include <userver/ugrpc/server/server.hpp>
namespace broker {
// Тестовая ручка доказывает передачу одной трассы через HTTP и gRPC.
class TraceProbe final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "handler-trace-probe";
    TraceProbe(const userver::components::ComponentConfig&,
               const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
                                   userver::server::request::RequestContext&) const override;

  private:
    userver::ugrpc::client::ClientFactory& factory_;
    userver::ugrpc::server::Server& grpc_;
};
// Управление тестовой зависимостью доступно только на внутреннем listener testsuite.
class ReadinessControl final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "handler-readiness-control";
    ReadinessControl(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
        : HttpHandlerBase(config, context), ready_(std::make_shared<std::atomic<bool>>(true)),
          registration_(context.FindComponent<platform::Readiness>().Register(
              "test-dependency", [ready = ready_] {
                  return platform::DependencyStatus{ready->load(),
                                                    "test dependency is unavailable"};
              })) {}

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                   userver::server::request::RequestContext&) const override {
        ready_->store(request.GetArg("ready") != "false");
        return "ok\n";
    }

  private:
    std::shared_ptr<std::atomic<bool>> ready_;
    platform::Readiness::Registration registration_;
};
}  // namespace broker

#pragma once
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
}  // namespace broker

#include <chrono>
#include <handlers/hello_client.usrv.pb.hpp>
#include <trace_probe.hpp>
#include <userver/components/component_context.hpp>
#include <userver/tracing/span.hpp>
#include <userver/ugrpc/client/client_factory_component.hpp>
#include <userver/ugrpc/server/server_component.hpp>
namespace broker {
TraceProbe::TraceProbe(const userver::components::ComponentConfig& config,
                       const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      factory_(
          context
              .FindComponent<userver::ugrpc::client::ClientFactoryComponent>("grpc-probe-factory")
              .GetFactory()),
      grpc_(context.FindComponent<userver::ugrpc::server::ServerComponent>().GetServer()) {}
std::string TraceProbe::HandleRequestThrow(const userver::server::http::HttpRequest&,
                                           userver::server::request::RequestContext&) const {
    auto client = factory_.MakeClient<handlers::api::HelloServiceClient>(
        "trace-probe", "localhost:" + std::to_string(grpc_.GetPort()));
    handlers::api::HelloRequest request;
    request.set_name("trace-probe");
    userver::ugrpc::client::CallOptions options;
    options.SetTimeout(std::chrono::seconds{2});
    client.SayHello(request, std::move(options));
    return std::string(userver::tracing::Span::CurrentSpan().GetTraceId());
}
}  // namespace broker

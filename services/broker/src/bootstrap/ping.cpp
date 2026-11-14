#include <bootstrap/ping.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_response.hpp>
namespace broker {
PingReady::PingReady(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), state_(context), deps_(ResolveDeps(context)) {}

std::string PingReady::HandleRequestThrow(const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
    if (state_.GetServiceLifetimeStage() != userver::components::ServiceLifetimeStage::kRunning ||
        state_.IsAnyComponentInFatalState()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
        return "not ready\n";
    }
    try {
        const auto readiness = deps_.readiness->GetStatus();
        if (!readiness.Ready) {
            request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
            return readiness.Reason + "\n";
        }
        return "ready\n";
    } catch (const std::exception&) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
        return "dependency unavailable\n";
    }
}
}  // namespace broker

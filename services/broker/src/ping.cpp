#include <ping.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
namespace broker {
std::string Ping::HandleRequestThrow(const userver::server::http::HttpRequest&,
                                     userver::server::request::RequestContext&) const {
    return "alive\n";
}
PingReady::PingReady(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), state_(context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()) {}
std::string PingReady::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                          userver::server::request::RequestContext&) const {
    if (state_.GetServiceLifetimeStage() != userver::components::ServiceLifetimeStage::kRunning ||
        state_.IsAnyComponentInFatalState()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
        return "not ready\n";
    }
    try {
        pg_->Execute(userver::storages::postgres::ClusterHostType::kMaster, "SELECT 1");
        return "ready\n";
    } catch (const std::exception&) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
        return "dependency unavailable\n";
    }
}
}  // namespace broker

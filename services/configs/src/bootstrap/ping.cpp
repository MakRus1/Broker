#include <bootstrap/ping.hpp>
#include <configs/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/server/http/http_response.hpp>
// SERVICE_PING_INCLUDES_BEGIN
#include <userver/storages/postgres/cluster.hpp>
// SERVICE_PING_INCLUDES_END
namespace configs {
std::string Ping::HandleRequestThrow(const userver::server::http::HttpRequest&,
                                     userver::server::request::RequestContext&) const {
    return "alive\n";
}
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
        // SERVICE_PING_CHECK_BEGIN
        deps_.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                                  "SELECT 1");
        // SERVICE_PING_CHECK_END
        // Проверяем наличие схемы, а не только доступность PostgreSQL.
        deps_.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                                  ::configs::sql::kLastRevision);
        return "ready\n";
    } catch (const std::exception&) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kServiceUnavailable);
        return "dependency unavailable\n";
    }
}
}  // namespace configs

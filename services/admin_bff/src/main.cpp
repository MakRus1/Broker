#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/ugrpc/server/component_list.hpp>
#include <userver/utils/daemon_run.hpp>

#include <bootstrap/ping.hpp>
#include <platform/auth/component.hpp>
#include <platform/browser/assets.hpp>
#include <platform/browser/login.hpp>
#include <platform/browser/session.hpp>
#include <platform/browser/sessions.hpp>
#include <platform/telemetry.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/fs_cache.hpp>
#include <userver/otlp/logs/component.hpp>
#include <userver/server/handlers/server_monitor.hpp>
#include <userver/ugrpc/client/client_factory_component.hpp>
#include <userver/ugrpc/client/component_list.hpp>

#include <openapi/handlers.hpp>

// SERVICE_INCLUDES_BEGIN
#include <clients/config_admin/client.hpp>
#include <clients/finance/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <clients/transportations/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>

// SERVICE_INCLUDES_END

int main(int argc, char* argv[]) {
    auto component_list =
        userver::components::MinimalServerComponentList()
            .Append<userver::components::TestsuiteSupport>()
            .AppendComponentList(userver::clients::http::ComponentList())
            .Append<userver::clients::dns::Component>()
            .Append<userver::server::handlers::TestsControl>()
            .Append<userver::congestion_control::Component>()
            .Append<platform::HttpTelemetryFactory>()
            .Append<platform::auth::Component>()
            .Append<userver::components::HttpClient>("auth-http-client")
            .Append<platform::browser::Sessions>()
            .Append<userver::components::FsCache>("browser-assets")
            .Append<platform::browser::BrowserSessionFactory>()
            .Append<platform::HttpTelemetryPipeline>()
            .Append<platform::browser::Login>()
            .Append<platform::browser::LoginCallback>()
            .Append<platform::browser::Assets>()
            .Append<platform::browser::Assets>("browser-index")
            .Append<platform::browser::Assets>("browser-index-html")
            .Append<admin_bff::Ping>()
            .Append<admin_bff::PingReady>()
            .Append<userver::server::handlers::ServerMonitor>()
            .AppendComponentList(userver::ugrpc::client::MinimalComponentList())
            .Append<userver::ugrpc::client::ClientFactoryComponent>("grpc-otlp-factory")
            .Append<userver::otlp::LoggerComponent>();

    // SERVICE_REGISTRATION_BEGIN
    component_list.Append<platform::Readiness>().Append<platform::HttpReadinessFactory>();
    component_list.AppendComponentList(userver::dynamic_config::updater::ComponentList());
    component_list.Append<platform::configs::DeliverySink<::configs::DeliveryContract>>();
    component_list.Append<platform::configs::ConfigReadiness>();
    component_list.Append<admin_bff::clients::config_admin::Component>();
    component_list.Append<admin_bff::clients::finance::Component>();
    component_list.Append<admin_bff::clients::organizations::Component>();
    component_list.Append<admin_bff::clients::shipments::Component>();
    component_list.Append<admin_bff::clients::transport_locations::Component>();
    component_list.Append<admin_bff::clients::transportations::Component>();
    // SERVICE_REGISTRATION_END

    component_list = admin_bff::openapi::AppendGeneratedHandlers(component_list);

    return userver::utils::DaemonMain(argc, argv, component_list);
}

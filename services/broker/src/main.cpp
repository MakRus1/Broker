#include <cstdlib>
#include <string_view>
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

#include <platform/telemetry.hpp>
#ifdef BROKER_TRACE_TESTS
#include <trace_probe.hpp>
#endif
#include <bootstrap/ping.hpp>
#include <userver/otlp/logs/component.hpp>
#include <userver/server/handlers/server_monitor.hpp>
#include <userver/ugrpc/client/client_factory_component.hpp>
#include <userver/ugrpc/client/component_list.hpp>

#include <openapi/handlers.hpp>
#include <transport/grpc/hello_grpc.hpp>

// SERVICE_INCLUDES_BEGIN
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/grpc_readiness.hpp>
#include <platform/grpc_telemetry.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
#include <userver/storages/postgres/component.hpp>

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
            .Append<platform::HttpTelemetryPipeline>()
            .Append<broker::Ping>()
            .Append<broker::PingReady>()
            .Append<userver::server::handlers::ServerMonitor>()
            .AppendComponentList(userver::ugrpc::client::MinimalComponentList())
            .Append<userver::ugrpc::client::ClientFactoryComponent>("grpc-otlp-factory")
            .Append<userver::otlp::LoggerComponent>();

#ifdef BROKER_TRACE_TESTS
    // Testsuite включает вспомогательную ручку вместе с ее конфигурацией.
    const auto* trace_tests = std::getenv("BROKER_TRACE_TESTS");
    if (trace_tests && std::string_view{trace_tests} == "1") {
        component_list.Append<broker::TraceProbe>()
            .Append<broker::ReadinessControl>()
            .Append<userver::ugrpc::client::ClientFactoryComponent>("grpc-probe-factory");
    }
#endif

    // SERVICE_REGISTRATION_BEGIN
    component_list.Append<platform::Readiness>().Append<platform::HttpReadinessFactory>();
    component_list.AppendComponentList(userver::dynamic_config::updater::ComponentList());
    component_list.Append<platform::configs::DeliverySink<::configs::DeliveryContract>>();
    component_list.Append<platform::configs::ConfigReadiness>();
    component_list.Append<userver::components::Postgres>("postgres-broker");
    component_list.AppendComponentList(userver::ugrpc::server::MinimalComponentList())
        .Append<platform::GrpcTelemetryFactory>()
        .Append<platform::GrpcReadinessFactory>();
    // SERVICE_REGISTRATION_END

    component_list.Append<broker::HelloGrpc>();
    component_list = broker::openapi::AppendGeneratedHandlers(component_list);

    return userver::utils::DaemonMain(argc, argv, component_list);
}

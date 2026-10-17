#include <openapi/handlers.hpp>
#include <platform/telemetry.hpp>
#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>
int main(int argc, char* argv[]) {
    auto list = userver::components::MinimalServerComponentList()
                    .AppendComponentList(userver::clients::http::ComponentList())
                    .Append<userver::clients::dns::Component>()
                    .Append<platform::HttpTelemetryFactory>()
                    .Append<platform::HttpTelemetryPipeline>()
                    .Append<userver::components::TestsuiteSupport>()
                    .Append<userver::server::handlers::TestsControl>();
    list = contract_service::openapi::AppendGeneratedHandlers(list);
    return userver::utils::DaemonMain(argc, argv, list);
}

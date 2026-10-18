#include "support/client_probe.hpp"
#include <openapi/handlers.hpp>
#include <platform/telemetry.hpp>
#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>
// SERVICE_INCLUDES_BEGIN
#include <clients/broker/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/runtime.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
// SERVICE_INCLUDES_END
int main(int argc, char* argv[]) {
    auto component_list = userver::components::MinimalServerComponentList()
                              .AppendComponentList(userver::clients::http::ComponentList())
                              .Append<userver::clients::dns::Component>()
                              .Append<platform::HttpTelemetryFactory>()
                              .Append<platform::HttpTelemetryPipeline>()
                              .Append<userver::components::TestsuiteSupport>()
                              .Append<userver::server::handlers::TestsControl>();
    component_list.Append<contract_service::ClientProbe>();
    // SERVICE_REGISTRATION_BEGIN
    component_list.AppendComponentList(userver::dynamic_config::updater::ComponentList());
    component_list.Append<contract_service::clients::broker::Component>();
    // SERVICE_REGISTRATION_END
    component_list = contract_service::openapi::AppendGeneratedHandlers(component_list);
    return userver::utils::DaemonMain(argc, argv, component_list);
}

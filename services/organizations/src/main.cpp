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
#include <components/access.hpp>
#include <components/keycloak_users.hpp>
#include <components/members.hpp>
#include <components/organizations.hpp>
#include <components/permission_releases.hpp>
#include <components/registry_defaults.hpp>
#include <components/roles.hpp>

#include <platform/auth/service_token.hpp>
#include <platform/telemetry.hpp>
#include <userver/otlp/logs/component.hpp>
#include <userver/server/handlers/server_monitor.hpp>
#include <userver/ugrpc/client/client_factory_component.hpp>
#include <userver/ugrpc/client/component_list.hpp>

#include <openapi/handlers.hpp>

// SERVICE_INCLUDES_BEGIN
#include <platform/auth/component.hpp>
#include <platform/readiness.hpp>
#include <userver/clients/http/component.hpp>
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
            .Append<platform::auth::ServiceTokenProvider>()
            .Append<organizations::KeycloakUsers>()
            .Append<organizations::Ping>()
            .Append<organizations::PingReady>()
            .Append<userver::server::handlers::ServerMonitor>()
            .AppendComponentList(userver::ugrpc::client::MinimalComponentList())
            .Append<userver::ugrpc::client::ClientFactoryComponent>("grpc-otlp-factory")
            .Append<userver::otlp::LoggerComponent>();

    // SERVICE_REGISTRATION_BEGIN
    component_list.Append<platform::Readiness>().Append<platform::HttpReadinessFactory>();
    component_list.Append<platform::auth::Component>();
    component_list.Append<userver::components::HttpClient>("auth-http-client");
    component_list.Append<userver::components::Postgres>("postgres-organizations");
    // SERVICE_REGISTRATION_END

    component_list.Append<organizations::AccessComponent>();
    component_list.Append<organizations::MembersComponent>();
    component_list.Append<organizations::OrganizationsComponent>();
    component_list.Append<organizations::PermissionReleasesComponent>();
    component_list.Append<organizations::RegistryDefaultsComponent>();
    component_list.Append<organizations::RolesComponent>();
    component_list = organizations::openapi::AppendGeneratedHandlers(component_list);

    return userver::utils::DaemonMain(argc, argv, component_list);
}

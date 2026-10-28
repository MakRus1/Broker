#pragma once

#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/counterparties/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace transportations {

struct Deps {
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    clients::counterparties::Client counterparties;
    clients::shipments::Client shipments;
    clients::transport_locations::Client transport_locations;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .counterparties =
            component_context.FindComponent<clients::counterparties::Component>().GetClient(),
        .shipments = component_context.FindComponent<clients::shipments::Component>().GetClient(),
        .transport_locations =
            component_context.FindComponent<clients::transport_locations::Component>().GetClient(),
        .pg_cluster = component_context
                          .FindComponent<userver::components::Postgres>("postgres-transportations")
                          .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace transportations

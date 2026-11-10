#pragma once
#include <components/locations.hpp>
#include <components/railway_stations.hpp>

#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace transport_locations {

struct Deps {
    LocationsComponent* locations;
    RailwayStationsComponent* stations;
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .locations = &component_context.FindComponent<LocationsComponent>(),
        .stations = &component_context.FindComponent<RailwayStationsComponent>(),
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .pg_cluster =
            component_context
                .FindComponent<userver::components::Postgres>("postgres-transport_locations")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace transport_locations

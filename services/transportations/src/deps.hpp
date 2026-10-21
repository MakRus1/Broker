#pragma once

#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <platform/readiness.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace transportations {

struct Deps {
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .pg_cluster = component_context
                          .FindComponent<userver::components::Postgres>("postgres-transportations")
                          .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace transportations

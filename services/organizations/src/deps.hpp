#pragma once

#include <components/users.hpp>
#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <platform/readiness.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace organizations {

struct Deps {
    Users* users;
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .users = &component_context.FindComponent<Users>(),
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .pg_cluster =
            component_context.FindComponent<userver::components::Postgres>("postgres-organizations")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace organizations

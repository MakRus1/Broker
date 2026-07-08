#pragma once

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

namespace broker::views::hello_postgres {

struct Deps {
    userver::storages::postgres::ClusterPtr pg_cluster;
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .pg_cluster = component_context
            .FindComponent<userver::components::Postgres>("postgres-db-1")
            .GetCluster(),
    };
}

}  // namespace broker::views::hello_postgres

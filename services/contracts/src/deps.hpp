#pragma once
#include <components/contract_registry.hpp>
#include <components/contracts.hpp>
#include <components/numbering.hpp>

#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace contracts {

struct Deps {
    ContractsComponent* contracts;
    ContractRegistry* registry;
    NumberingComponent* numbering;
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    clients::counterparties::Client counterparties;
    clients::organizations::Client organizations;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .contracts = &component_context.FindComponent<ContractsComponent>(),
        .registry = &component_context.FindComponent<ContractRegistry>(),
        .numbering = &component_context.FindComponent<NumberingComponent>(),
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .counterparties =
            component_context.FindComponent<clients::counterparties::Component>().GetClient(),
        .organizations =
            component_context.FindComponent<clients::organizations::Component>().GetClient(),
        .pg_cluster =
            component_context.FindComponent<userver::components::Postgres>("postgres-contracts")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace contracts

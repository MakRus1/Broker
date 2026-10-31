#pragma once

#include <components/exchange_rates.hpp>
#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/contracts/client.hpp>
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transportations/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace finance {

struct Deps {
    ExchangeRates& exchange_rates;
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    clients::contracts::Client contracts;
    clients::counterparties::Client counterparties;
    clients::organizations::Client organizations;
    clients::shipments::Client shipments;
    clients::transportations::Client transportations;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .exchange_rates = component_context.FindComponent<ExchangeRates>(),
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .contracts = component_context.FindComponent<clients::contracts::Component>().GetClient(),
        .counterparties =
            component_context.FindComponent<clients::counterparties::Component>().GetClient(),
        .organizations =
            component_context.FindComponent<clients::organizations::Component>().GetClient(),
        .shipments = component_context.FindComponent<clients::shipments::Component>().GetClient(),
        .transportations =
            component_context.FindComponent<clients::transportations::Component>().GetClient(),
        .pg_cluster =
            component_context.FindComponent<userver::components::Postgres>("postgres-finance")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace finance

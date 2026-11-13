#pragma once
#include <components/cargo_suggestions.hpp>
#include <components/container_plan.hpp>
#include <components/settings.hpp>
#include <components/shipment_registry.hpp>
#include <components/shipment_routes.hpp>
#include <components/shipments.hpp>
#include <components/transportation_operations.hpp>

#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/counterparties/client.hpp>
#include <clients/notifications/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/transport_locations/client.hpp>
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

namespace shipments {

struct Deps {
    ShipmentsComponent* shipments;
    ShipmentRegistryComponent* shipment_registry;
    SettingsComponent* settings;
    ShipmentRoutes* shipment_routes;
    ContainerPlanComponent* container_plan;
    TransportationOperationsComponent* transportation_operations;
    CargoSuggestionsComponent* cargo_suggestions;

    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    clients::counterparties::Client counterparties;
    clients::notifications::Client notifications;
    clients::organizations::Client organizations;
    clients::transport_locations::Client transport_locations;
    clients::transportations::Client transportations;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .shipments = &component_context.FindComponent<ShipmentsComponent>(),
        .shipment_registry = &component_context.FindComponent<ShipmentRegistryComponent>(),
        .settings = &component_context.FindComponent<SettingsComponent>(),
        .shipment_routes = &component_context.FindComponent<ShipmentRoutes>(),
        .container_plan = &component_context.FindComponent<ContainerPlanComponent>(),
        .transportation_operations =
            &component_context.FindComponent<TransportationOperationsComponent>(),
        .cargo_suggestions = &component_context.FindComponent<CargoSuggestionsComponent>(),

        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .counterparties =
            component_context.FindComponent<clients::counterparties::Component>().GetClient(),
        .notifications =
            component_context.FindComponent<clients::notifications::Component>().GetClient(),
        .organizations =
            component_context.FindComponent<clients::organizations::Component>().GetClient(),
        .transport_locations =
            component_context.FindComponent<clients::transport_locations::Component>().GetClient(),
        .transportations =
            component_context.FindComponent<clients::transportations::Component>().GetClient(),
        .pg_cluster =
            component_context.FindComponent<userver::components::Postgres>("postgres-shipments")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace shipments

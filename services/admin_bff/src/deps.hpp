#pragma once

#include <components/session.hpp>
#include <platform/browser/sessions.hpp>
#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/config_admin/client.hpp>
#include <clients/contracts/client.hpp>
#include <clients/finance/client.hpp>
#include <clients/localization/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/sales/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <clients/transportations/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace admin_bff {

struct Deps {
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::configs::Source configs;
    clients::config_admin::Client config_admin;
    clients::contracts::Client contracts;
    clients::finance::Client finance;
    clients::localization::Client localization;
    clients::organizations::Client organizations;
    clients::sales::Client sales;
    clients::shipments::Client shipments;
    clients::transport_locations::Client transport_locations;
    clients::transportations::Client transportations;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
    AdminSessionComponent& admin_session;
    platform::browser::Sessions* sessions;
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        // SERVICE_DEPS_INIT_BEGIN
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .config_admin =
            component_context.FindComponent<clients::config_admin::Component>().GetClient(),
        .contracts = component_context.FindComponent<clients::contracts::Component>().GetClient(),
        .finance = component_context.FindComponent<clients::finance::Component>().GetClient(),
        .localization =
            component_context.FindComponent<clients::localization::Component>().GetClient(),
        .organizations =
            component_context.FindComponent<clients::organizations::Component>().GetClient(),
        .sales = component_context.FindComponent<clients::sales::Component>().GetClient(),
        .shipments = component_context.FindComponent<clients::shipments::Component>().GetClient(),
        .transport_locations =
            component_context.FindComponent<clients::transport_locations::Component>().GetClient(),
        .transportations =
            component_context.FindComponent<clients::transportations::Component>().GetClient(),
        .pg_cluster =
            component_context.FindComponent<userver::components::Postgres>("postgres-admin-bff")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
        .admin_session = component_context.FindComponent<AdminSessionComponent>(),
        .sessions = &component_context.FindComponent<platform::browser::Sessions>(),
    };
}

}  // namespace admin_bff

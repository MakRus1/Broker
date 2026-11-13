#pragma once
#include <components/access.hpp>
#include <components/members.hpp>
#include <components/organizations.hpp>
#include <components/permission_releases.hpp>
#include <components/registry_defaults.hpp>
#include <components/roles.hpp>

#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <platform/readiness.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace organizations {

struct Deps {
    AccessComponent* access;
    MembersComponent* members;
    OrganizationsComponent* organizations;
    PermissionReleasesComponent* permission_releases;
    RegistryDefaultsComponent* registry_defaults;
    RolesComponent* roles;

    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    userver::storages::postgres::ClusterPtr pg_cluster;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .access = &component_context.FindComponent<AccessComponent>(),
        .members = &component_context.FindComponent<MembersComponent>(),
        .organizations = &component_context.FindComponent<OrganizationsComponent>(),
        .permission_releases = &component_context.FindComponent<PermissionReleasesComponent>(),
        .registry_defaults = &component_context.FindComponent<RegistryDefaultsComponent>(),
        .roles = &component_context.FindComponent<RolesComponent>(),

        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .pg_cluster =
            component_context.FindComponent<userver::components::Postgres>("postgres-organizations")
                .GetCluster(),
        // SERVICE_DEPS_INIT_END
    };
}

}  // namespace organizations

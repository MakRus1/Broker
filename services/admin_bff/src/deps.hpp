#pragma once

#include <components/sessions.hpp>
#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/config_admin/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>

// SERVICE_DEPS_INCLUDES_END

namespace admin_bff {

struct Deps {
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    clients::config_admin::Client config_admin;
    // SERVICE_DEPS_FIELDS_END
    Sessions* sessions;
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .config_admin =
            component_context.FindComponent<clients::config_admin::Component>().GetClient(),
        // SERVICE_DEPS_INIT_END
        .sessions = &component_context.FindComponent<Sessions>(),
    };
}

}  // namespace admin_bff

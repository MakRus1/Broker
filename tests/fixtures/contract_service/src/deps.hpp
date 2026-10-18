#pragma once
#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/broker/client.hpp>
#include <configs/keys.hpp>
#include <platform/configs/components.hpp>
#include <platform/configs/runtime.hpp>
#include <platform/readiness.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component_list.hpp>

// SERVICE_DEPS_INCLUDES_END
namespace contract_service {
struct Deps {
    // SERVICE_DEPS_FIELDS_BEGIN
    platform::Readiness* readiness;
    platform::configs::Source configs;
    clients::broker::Client broker;
    // SERVICE_DEPS_FIELDS_END
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        // SERVICE_DEPS_INIT_BEGIN
        .readiness = &component_context.FindComponent<platform::Readiness>(),
        .configs =
            platform::configs::Source{
                component_context.FindComponent<userver::components::DynamicConfig>().GetSource()},
        .broker = component_context.FindComponent<clients::broker::Component>().GetClient(),
        // SERVICE_DEPS_INIT_END
    };
}
}  // namespace contract_service

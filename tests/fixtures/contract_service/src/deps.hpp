#pragma once
#include <userver/components/component_context.hpp>
// SERVICE_DEPS_INCLUDES_BEGIN
#include <clients/broker/client.hpp>
// SERVICE_DEPS_INCLUDES_END
namespace contract_service {
struct Deps {
    // SERVICE_DEPS_FIELDS_BEGIN
    clients::broker::Client broker;
    // SERVICE_DEPS_FIELDS_END
};
inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        // SERVICE_DEPS_INIT_BEGIN
        .broker = component_context.FindComponent<clients::broker::Component>().GetClient(),
        // SERVICE_DEPS_INIT_END
    };
}
}  // namespace contract_service

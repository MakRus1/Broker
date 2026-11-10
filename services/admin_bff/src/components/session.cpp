#include <components/session.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace admin_bff {
AdminSessionComponent::AdminSessionComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

userver::yaml_config::Schema AdminSessionComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::AdminSession AdminSessionComponent::Get(
    const platform::browser::BrowserSession& session) const {
    const auto access =
        organizations_.ApiPlatformAccess({.body = {.permission = "authenticated"}}, session.Access);
    return {.actor = session.User.ActorId(),
        .csrf_token = session.Csrf,
        .display_name = session.User.DisplayName,
        .permissions = access.permissions,
        .subject = session.User.Subject};
}
}  // namespace admin_bff

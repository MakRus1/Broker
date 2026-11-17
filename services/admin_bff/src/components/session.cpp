#include <components/session.hpp>
#include <userver/components/component_context.hpp>

namespace admin_bff {
AdminSessionComponent::AdminSessionComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

models::AdminSession AdminSessionComponent::GetAdminSession(
    const platform::browser::BrowserSession& session) const {
    const auto access =
        organizations_.ApiPlatformAccess({.body = {.permission = "authenticated"}}, session.Access);
    const auto profile = organizations_.ApiOrganizationsProfile({}, session.Access);
    return {.actor = session.User.ActorId(),
        .csrf_token = session.Csrf,
        .display_name = profile.display_name.value_or(session.User.DisplayName),
        .permissions = access.permissions,
        .subject = session.User.Subject};
}
}  // namespace admin_bff

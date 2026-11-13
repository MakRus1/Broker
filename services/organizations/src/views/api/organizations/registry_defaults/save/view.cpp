#include <components/registry_defaults.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/organizations/registry_defaults/save/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.registry_defaults->SaveRoleRegistryDefault(request.body, identity);

    return Response200{.item = std::move(result.item)};
}
}  // namespace organizations::views::api::organizations::registry_defaults::save

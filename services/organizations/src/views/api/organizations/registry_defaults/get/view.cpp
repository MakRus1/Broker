#include <components/registry_defaults.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/organizations/registry_defaults/get/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.registry_defaults->GetOwnRegistryDefaults(request.body, identity);

    return Response200{.items = std::move(result.items)};
}
}  // namespace organizations::views::api::organizations::registry_defaults::get

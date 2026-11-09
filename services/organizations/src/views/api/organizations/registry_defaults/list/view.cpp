#include <components/registry_defaults.hpp>
#include <utility>
#include <views/api/organizations/registry_defaults/list/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.registry_defaults->List(request.body, context);
    return Response200{.items = std::move(result.items), .roles = std::move(result.roles)};
}
}  // namespace organizations::views::api::organizations::registry_defaults::list

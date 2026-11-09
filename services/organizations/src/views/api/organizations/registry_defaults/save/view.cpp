#include <components/registry_defaults.hpp>
#include <utility>
#include <views/api/organizations/registry_defaults/save/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.registry_defaults->Save(request.body, context);
    return Response200{.item = std::move(result.item)};
}
}  // namespace organizations::views::api::organizations::registry_defaults::save

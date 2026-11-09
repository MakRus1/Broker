#include <components/registry_defaults.hpp>
#include <utility>
#include <views/api/organizations/registry_defaults/get/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.registry_defaults->Get(request.body, context);
    return Response200{.items = std::move(result.items)};
}
}  // namespace organizations::views::api::organizations::registry_defaults::get

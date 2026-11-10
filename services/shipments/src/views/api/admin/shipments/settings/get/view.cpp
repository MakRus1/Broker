#include <components/settings.hpp>
#include <utility>
#include <views/api/admin/shipments/settings/get/view.hpp>

namespace shipments::views::api::admin::shipments::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.settings->GetBusiness(request.body, context);
    return Response200{.allowed_container_types = std::move(result.allowed_container_types),
        .allowed_route_types = std::move(result.allowed_route_types),
        .planned_containers_allowed = std::move(result.planned_containers_allowed),
        .revision = std::move(result.revision),
        .rules = std::move(result.rules)};
}
}  // namespace shipments::views::api::admin::shipments::settings::get

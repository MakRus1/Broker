#include <components/settings.hpp>
#include <utility>
#include <views/api/admin/shipments/settings/get/view.hpp>
#include <views/api/admin/shipments/settings/require_administrator.hpp>

namespace shipments::views::api::admin::shipments::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& organization = request.body.organization_id;
    RequireAdministrator(deps.organizations, context, organization);
    auto result = deps.settings->GetBusinessSettings(organization);
    return Response200{.allowed_container_types = std::move(result.allowed_container_types),
        .allowed_route_types = std::move(result.allowed_route_types),
        .planned_containers_allowed = std::move(result.planned_containers_allowed),
        .revision = std::move(result.revision),
        .rules = std::move(result.rules)};
}
}  // namespace shipments::views::api::admin::shipments::settings::get

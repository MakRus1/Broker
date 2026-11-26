#include <components/settings.hpp>
#include <helpers/organization_admin.hpp>
#include <utility>
#include <views/api/admin/shipments/settings/get/view.hpp>

namespace shipments::views::api::admin::shipments::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    helpers::RequireOrganizationAdministrator(
        deps.organizations, context, request.body.organization_id);

    const auto organization = request.body.organization_id;
    auto result = deps.settings->GetBusinessSettings(organization);
    return Response200{.allowed_route_types = std::move(result.allowed_route_types),
        .planned_containers_allowed = std::move(result.planned_containers_allowed),
        .revision = std::move(result.revision),
        .rules = std::move(result.rules)};
}
}  // namespace shipments::views::api::admin::shipments::settings::get

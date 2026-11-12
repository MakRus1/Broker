#include <components/settings.hpp>
#include <utility>
#include <views/api/admin/shipments/settings/require_administrator.hpp>
#include <views/api/admin/shipments/settings/update/view.hpp>

namespace shipments::views::api::admin::shipments::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& organization = request.body.organization_id;
    RequireAdministrator(deps.organizations, context, organization);
    auto result = deps.settings->SaveBusinessSettings(organization,
        platform::auth::GetIdentity(context), request.body.rules, request.body.expected_revision);
    return Response200{.allowed_container_types = std::move(result.allowed_container_types),
        .allowed_route_types = std::move(result.allowed_route_types),
        .planned_containers_allowed = std::move(result.planned_containers_allowed),
        .revision = std::move(result.revision),
        .rules = std::move(result.rules)};
}
}  // namespace shipments::views::api::admin::shipments::settings::update

#include <platform/browser/session.hpp>
#include <views/api/admin/shipments/settings/update/view.hpp>

namespace admin_bff::views::api::admin::shipments::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiAdminShipmentsSettingsUpdate;
    const auto result = deps.shipments.ApiAdminShipmentsSettingsUpdate(
        api::Request{.body = {.expected_revision = request.body.expected_revision,
                         .organization_id = request.body.organization_id,
                         .rules = {.container_types = request.body.rules.container_types,
                             .number_template = request.body.rules.number_template,
                             .planned_containers = request.body.rules.planned_containers,
                             .reset_number_yearly = request.body.rules.reset_number_yearly,
                             .route_types = request.body.rules.route_types,
                             .statuses = request.body.rules.statuses}}},
        session.Access);
    Response200 response{.allowed_container_types = result.allowed_container_types,
        .planned_containers_allowed = result.planned_containers_allowed,
        .revision = result.revision,
        .rules = {.container_types = result.rules.container_types,
            .number_template = result.rules.number_template,
            .planned_containers = result.rules.planned_containers,
            .reset_number_yearly = result.rules.reset_number_yearly,
            .route_types = result.rules.route_types,
            .statuses = result.rules.statuses}};
    for (const auto& route : result.allowed_route_types)
        response.allowed_route_types.push_back({.code = route.code, .name = route.name});
    return response;
}
}  // namespace admin_bff::views::api::admin::shipments::settings::update

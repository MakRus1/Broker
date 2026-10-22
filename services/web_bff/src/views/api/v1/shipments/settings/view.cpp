#include <platform/browser/session.hpp>
#include <views/api/v1/shipments/settings/view.hpp>

namespace web_bff::views::api::v1::shipments::settings {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentsSettings;
    const auto result = deps.shipments.ApiV1ShipmentsSettings(
        api::Request{.body = {.customer_id = request.body.customer_id}}, session.Access);
    return Response200{.container_types = result.container_types,
        .default_logist = result.default_logist,
        .number_template = result.number_template,
        .planned_containers = result.planned_containers,
        .reset_number_yearly = result.reset_number_yearly,
        .route_types = result.route_types,
        .statuses = result.statuses};
}
}  // namespace web_bff::views::api::v1::shipments::settings

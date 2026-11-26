#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/shipment_route_progress/view.hpp>

namespace web_bff::views::api::v1::transportations::shipment_route_progress {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.transportations.ApiV1TransportationsShipmentRouteProgress(
        {.body = {.shipment_id = request.body.shipment_id}}, session.Access);
    Response200 response{.total = result.total};
    for (const auto& stage : result.current)
        response.current.push_back(
            {.quantity = stage.quantity, .route_segment_id = stage.route_segment_id});
    return response;
}
}  // namespace web_bff::views::api::v1::transportations::shipment_route_progress

#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace web_bff::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentItemsList;
    const auto result = deps.shipments.ApiV1ShipmentItemsList(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{
        .items = Convert(result.items), .unplanned_items = Convert(result.unplanned_items)};
}
}  // namespace web_bff::views::api::v1::shipment_items::list

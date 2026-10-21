#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/shipment_items/update/view.hpp>

namespace web_bff::views::api::v1::shipment_items::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentItemsUpdate;
    const auto body = Convert(request.body);
    const auto result =
        deps.shipments.ApiV1ShipmentItemsUpdate(api::Request{.body = body}, session.Access);
    return Response200{.item = Convert(result.item)};
}
}  // namespace web_bff::views::api::v1::shipment_items::update

#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace web_bff::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentItemsAdd;
    const auto body =
        api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue());
    const auto result =
        deps.shipments.ApiV1ShipmentItemsAdd(api::Request{.body = body}, session.Access);
    return Response201{.item = Convert<models::ShipmentItem>(result.item)};
}
}  // namespace web_bff::views::api::v1::shipment_items::add

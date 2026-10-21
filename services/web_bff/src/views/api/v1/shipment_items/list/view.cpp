#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace web_bff::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentItemsList;
    const auto body =
        api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue());
    const auto result =
        deps.shipments.ApiV1ShipmentItemsList(api::Request{.body = body}, session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back(Convert<models::ShipmentItem>(item));
    return response;
}
}  // namespace web_bff::views::api::v1::shipment_items::list

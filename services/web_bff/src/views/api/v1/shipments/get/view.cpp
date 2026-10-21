#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/shipments/get/view.hpp>

namespace web_bff::views::api::v1::shipments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentsGet;
    const auto result = deps.shipments.ApiV1ShipmentsGet(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.shipment = Convert(result.shipment)};
}
}  // namespace web_bff::views::api::v1::shipments::get

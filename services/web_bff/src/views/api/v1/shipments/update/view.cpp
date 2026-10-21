#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/shipments/update/view.hpp>

namespace web_bff::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentsUpdate;
    const auto body =
        api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue());
    const auto result =
        deps.shipments.ApiV1ShipmentsUpdate(api::Request{.body = body}, session.Access);
    return Response200{.shipment = Convert<models::Shipment>(result.shipment)};
}
}  // namespace web_bff::views::api::v1::shipments::update

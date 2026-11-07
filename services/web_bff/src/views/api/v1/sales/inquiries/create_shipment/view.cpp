#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/create_shipment/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::create_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesCreateShipment;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesCreateShipment(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.item = ConvertSales<models::sales::Inquiry>(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::create_shipment

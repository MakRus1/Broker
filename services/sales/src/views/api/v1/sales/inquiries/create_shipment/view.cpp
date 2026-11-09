#include <components/inquiries.hpp>
#include <views/api/v1/sales/inquiries/create_shipment/view.hpp>

namespace sales::views::api::v1::sales::inquiries::create_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.item = deps.inquiries->CreateShipment(request.body, context)};
}
}  // namespace sales::views::api::v1::sales::inquiries::create_shipment

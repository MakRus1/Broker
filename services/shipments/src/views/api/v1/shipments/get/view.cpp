#include <components/shipments.hpp>
#include <utility>
#include <views/api/v1/shipments/get/view.hpp>

namespace shipments::views::api::v1::shipments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->GetShipment(request.body, context);
    return Response200{.shipment = std::move(result.shipment)};
}
}  // namespace shipments::views::api::v1::shipments::get

#include <components/shipments.hpp>
#include <utility>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->CreateShipment(request.body, context);
    return Response201{.shipment = std::move(result.shipment)};
}
}  // namespace shipments::views::api::v1::shipments::create

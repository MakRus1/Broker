#include <components/container_plan.hpp>
#include <utility>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace shipments::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.container_plan->Add(request.body, context);
    return Response201{.item = std::move(result.item)};
}
}  // namespace shipments::views::api::v1::shipment_items::add

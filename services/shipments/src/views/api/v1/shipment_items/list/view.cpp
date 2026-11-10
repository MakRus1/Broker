#include <components/container_plan.hpp>
#include <utility>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace shipments::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.container_plan->List(request.body, context);
    return Response200{
        .items = std::move(result.items), .unplanned_items = std::move(result.unplanned_items)};
}
}  // namespace shipments::views::api::v1::shipment_items::list

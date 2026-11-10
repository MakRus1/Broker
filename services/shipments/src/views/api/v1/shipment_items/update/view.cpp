#include <components/container_plan.hpp>
#include <utility>
#include <views/api/v1/shipment_items/update/view.hpp>

namespace shipments::views::api::v1::shipment_items::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.container_plan->Update(request.body, context);
    return Response200{.item = std::move(result.item)};
}
}  // namespace shipments::views::api::v1::shipment_items::update

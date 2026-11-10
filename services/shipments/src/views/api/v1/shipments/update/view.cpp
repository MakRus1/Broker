#include <components/shipments.hpp>
#include <utility>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->Update(request.body, context);
    return Response200{.shipment = std::move(result.shipment)};
}
}  // namespace shipments::views::api::v1::shipments::update

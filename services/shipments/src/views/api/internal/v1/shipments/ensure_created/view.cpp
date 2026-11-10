#include <components/shipments.hpp>
#include <utility>
#include <views/api/internal/v1/shipments/ensure_created/view.hpp>

namespace shipments::views::api::internal::v1::shipments::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->EnsureCreated(request.body, context);
    return Response200{.shipment = std::move(result.shipment)};
}
}  // namespace shipments::views::api::internal::v1::shipments::ensure_created

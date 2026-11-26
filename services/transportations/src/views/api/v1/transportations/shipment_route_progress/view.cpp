#include <components/transportations.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/shipment_route_progress/view.hpp>

namespace transportations::views::api::v1::transportations::shipment_route_progress {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto result =
        deps.transportations->GetShipmentRouteProgress(organization, request.body.shipment_id);
    return Response200{.current = std::move(result.current), .total = result.total};
}
}  // namespace transportations::views::api::v1::transportations::shipment_route_progress

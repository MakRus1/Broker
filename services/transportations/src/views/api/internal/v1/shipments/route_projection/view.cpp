#include <components/routes.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/internal/v1/shipments/route_projection/view.hpp>

namespace transportations::views::api::internal::v1::shipments::route_projection {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_CREATE);

    auto result = deps.routes->ApplyShipmentRouteProjection(request.body);

    return Response200{.applied = result.applied};
}
}  // namespace transportations::views::api::internal::v1::shipments::route_projection

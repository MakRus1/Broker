#include <components/routes.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportation_routes/usage/view.hpp>

namespace transportations::views::api::v1::transportation_routes::usage {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    auto result = deps.routes->FindRouteUsage(organization, request.body.shipment_id);

    return Response200{.segment_ids = std::move(result.segment_ids)};
}
}  // namespace transportations::views::api::v1::transportation_routes::usage

#include <components/routes.hpp>
#include <helpers/revision.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportation_routes/set/view.hpp>

namespace transportations::views::api::v1::transportation_routes::set {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    auto loaded =
        deps.transportations->FindTransportation(organization, request.body.transportation_id);
    helpers::CheckRevision(loaded.revision, request.body.expected_revision);

    const auto& access_token = platform::auth::GetAccessToken(context);
    const auto source =
        deps.route_integrations->GetShipmentContext(access_token, loaded.shipment_id);
    auto result = deps.routes->SaveRoute(
        organization, actor, access_token, request.body, source, std::move(loaded));

    return Response200{.transportation = std::move(result)};
}
}  // namespace transportations::views::api::v1::transportation_routes::set

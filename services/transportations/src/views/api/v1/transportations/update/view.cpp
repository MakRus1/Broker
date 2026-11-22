#include <components/inherited_route.hpp>
#include <components/route_validation.hpp>
#include <components/transportations.hpp>
#include <helpers/revision.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportations/update/view.hpp>

namespace transportations::views::api::v1::transportations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    auto transportation =
        deps.transportations->FindTransportation(organization, request.body.transportation_id);
    helpers::CheckRevision(transportation.revision, request.body.expected_revision);

    const auto& access_token = platform::auth::GetAccessToken(context);
    const auto source =
        deps.route_integrations->GetShipmentContext(access_token, transportation.shipment_id);

    auto& body = request.body;
    if (body.route_segments) {
        models::SetRouteRequest route;
        route.transportation_id = body.transportation_id;
        route.expected_revision = body.expected_revision;
        route.route_segments = *body.route_segments;
        route_rules::InheritRoute(transportation, source);
        route_rules::ValidateRoute(route);
        deps.route_integrations->PrepareRoute(access_token, route, transportation);
        body.route_segments = std::move(route.route_segments);
    }

    auto result =
        deps.transportations->UpdateTransportation(organization, actor, body, source, access_token);

    return Response200{.transportation = std::move(result)};
}
}  // namespace transportations::views::api::v1::transportations::update

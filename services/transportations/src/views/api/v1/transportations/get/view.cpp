#include <components/inherited_route.hpp>
#include <components/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportations/get/view.hpp>

namespace transportations::views::api::v1::transportations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    auto transportation =
        deps.transportations->FindTransportation(organization, request.body.transportation_id);

    const auto& access_token = platform::auth::GetAccessToken(context);
    const auto sources =
        deps.route_integrations->FindShipmentContexts(access_token, {transportation.shipment_id});
    const auto& source = route_rules::FindShipmentContext(sources, transportation.shipment_id);
    route_rules::InheritRoute(transportation, source);

    return Response200{.transportation = std::move(transportation)};
}
}  // namespace transportations::views::api::v1::transportations::get

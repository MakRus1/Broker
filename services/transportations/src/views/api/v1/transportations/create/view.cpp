#include <components/inherited_route.hpp>
#include <components/transportations.hpp>
#include <helpers/revision.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportations/create/view.hpp>

namespace transportations::views::api::v1::transportations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    const auto& access_token = platform::auth::GetAccessToken(context);
    const auto source =
        deps.route_integrations->GetShipmentContext(access_token, request.body.shipment_id);

    auto result =
        deps.transportations->CreateTransportation(organization, actor, request.body, source);

    return Response200{.transportation = std::move(result)};
}
}  // namespace transportations::views::api::v1::transportations::create

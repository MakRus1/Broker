#include <components/shipment_card.hpp>
#include <components/shipments.hpp>
#include <helpers/revision.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/text.hpp>
#include <utility>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    const auto& access_token = platform::auth::GetAccessToken(context);

    auto& body = request.body;
    if (body.logist.has_value())
        deps.shipment_card_integrations->ValidateLogist(
            organization, access_token, platform::TrimToOptional(body.logist.value()));

    if (body.route_segments.has_value()) {
        const auto previous = deps.shipments->GetShipment(organization, body.shipment_id);
        helpers::CheckRevision(previous.revision, body.expected_revision);
        auto& route = body.route_segments.value();
        deps.shipment_card_integrations->ResolveRoute(access_token, route, previous.route_segments);
        deps.shipment_card_integrations->CheckExistingRouteUsage(access_token, body.shipment_id,
            shipment_card::ChangedRouteSegments(previous.route_segments, route));
    }

    return Response200{.shipment = deps.shipments->UpdateShipment(organization, actor, body)};
}
}  // namespace shipments::views::api::v1::shipments::update

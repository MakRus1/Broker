#include <components/shipments.hpp>
#include <helpers/revision.hpp>
#include <helpers/shipment_card.hpp>
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
        deps.shipments->ValidateLogist(
            organization, access_token, platform::TrimToOptional(body.logist.value()));

    std::optional<std::int64_t> route_usage_version;
    if (body.route_segments.has_value()) {
        route_usage_version = deps.shipments->GetRouteUsageVersion(organization, body.shipment_id);
        const auto previous = deps.shipments->GetShipment(organization, body.shipment_id);
        helpers::CheckRevision(previous.revision, body.expected_revision);
        auto& route = body.route_segments.value();
        deps.shipment_routes->ResolveRoute(access_token, route, previous.route_segments);
        deps.shipment_routes->CheckExistingRouteUsage(access_token, body.shipment_id,
            helpers::ChangedRouteSegments(previous.route_segments, route));
    }

    return Response200{
        .shipment = deps.shipments->UpdateShipment(organization, actor, body, route_usage_version)};
}
}  // namespace shipments::views::api::v1::shipments::update

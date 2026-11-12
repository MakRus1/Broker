#include <components/inherited_route.hpp>
#include <components/transportation_registry.hpp>
#include <components/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <utility>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);
    auto result = deps.transportation_registry->ListTransportations(organization, request.body);
    std::set<std::string> shipment_ids;
    for (const auto& item : result.items)
        shipment_ids.insert(item.shipment_id);

    const auto sources = deps.route_integrations->FindShipmentContexts(
        platform::auth::GetAccessToken(context), {shipment_ids.begin(), shipment_ids.end()});
    for (auto& item : result.items)
        route_rules::InheritRoute(
            item, route_rules::FindShipmentContext(sources, item.shipment_id));

    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace transportations::views::api::v1::transportations::list

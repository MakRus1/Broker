#include <components/shipment_registry.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/list/view.hpp>

namespace shipments::views::api::v1::shipments::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto result = deps.shipment_registry->FindShipments(request.body, organization);
    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace shipments::views::api::v1::shipments::list

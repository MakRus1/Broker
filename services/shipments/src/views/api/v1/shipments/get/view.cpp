#include <components/shipments.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/get/view.hpp>

namespace shipments::views::api::v1::shipments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{
        .shipment = deps.shipments->GetShipment(organization, request.body.shipment_id)};
}
}  // namespace shipments::views::api::v1::shipments::get

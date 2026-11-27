#include <line_operations/voyages.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/voyages/by_shipment/view.hpp>

namespace shipments::views::api::v1::voyages::by_shipment {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto org = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto voyage = line_operations::ShipmentVoyages(*deps.pg_cluster, org, request.body.shipment_id);
    return Response200{.items = std::move(voyage.items)};
}
}  // namespace shipments::views::api::v1::voyages::by_shipment

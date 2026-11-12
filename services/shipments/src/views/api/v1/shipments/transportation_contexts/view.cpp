#include <components/shipments.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/transportation_contexts/view.hpp>

namespace shipments::views::api::v1::shipments::transportation_contexts {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto shipments = deps.shipments->GetShipments(organization, request.body.shipment_ids);
    Response200 result;
    result.items.reserve(shipments.size());
    for (auto& shipment : shipments) {
        result.items.push_back(models::TransportationContext{
            .carriage_terms = shipment.carriage_terms.value_or(std::nullopt),
            .incoterms = shipment.incoterms.value_or(std::nullopt),
            .loading = shipment.loading.value_or(std::nullopt),
            .revision = shipment.revision,
            .route_segments = std::move(shipment.route_segments),
            .shipment_id = std::move(shipment.id)});
    }
    return result;
}
}  // namespace shipments::views::api::v1::shipments::transportation_contexts

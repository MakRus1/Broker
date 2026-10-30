#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipments/transportation_contexts/view.hpp>

namespace shipments::views::api::v1::shipments::transportation_contexts {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = deps.pg_cluster
                          ->Execute(pg::ClusterHostType::kMaster, sql::kTransportationContexts,
                              organization, request.body.shipment_ids)
                          .AsContainer<std::vector<pgtypes::ShipmentV4>>();
    std::vector<models::TransportationContext> items;
    for (const auto& row : rows) {
        const auto shipment = Convert(row);
        items.push_back(models::TransportationContext{
            .carriage_terms = shipment.carriage_terms.value_or(std::nullopt),
            .incoterms = shipment.incoterms.value_or(std::nullopt),
            .loading = shipment.loading.value_or(std::nullopt),
            .revision = shipment.revision,
            .route_segments = shipment.route_segments,
            .shipment_id = shipment.id});
    }
    return Response200{.items = std::move(items)};
}
}  // namespace shipments::views::api::v1::shipments::transportation_contexts

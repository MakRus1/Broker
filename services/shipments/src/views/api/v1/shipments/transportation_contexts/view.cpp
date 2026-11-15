#include <components/shipments.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipments/transportation_contexts/view.hpp>

namespace shipments::views::api::v1::shipments::transportation_contexts {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{.items = deps.shipments->GetTransportationContexts(
                           organization, request.body.shipment_ids)};
}
}  // namespace shipments::views::api::v1::shipments::transportation_contexts

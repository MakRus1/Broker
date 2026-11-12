#include <components/shipment_registry.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/filter_values/view.hpp>

namespace shipments::views::api::v1::shipments::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto result = deps.shipment_registry->GetShipmentFilterValues(request.body, organization);
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace shipments::views::api::v1::shipments::filter_values

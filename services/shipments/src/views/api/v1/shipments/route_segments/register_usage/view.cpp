#include <components/shipments.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/route_segments/register_usage/view.hpp>

namespace shipments::views::api::v1::shipments::route_segments::register_usage {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    deps.shipments->RegisterRouteUsage(organization, request.body);
    return Response200{.registered = true};
}
}  // namespace shipments::views::api::v1::shipments::route_segments::register_usage

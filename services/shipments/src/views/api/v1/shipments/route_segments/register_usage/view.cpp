#include <components/shipments.hpp>
#include <utility>
#include <views/api/v1/shipments/route_segments/register_usage/view.hpp>

namespace shipments::views::api::v1::shipments::route_segments::register_usage {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->RegisterRouteUsage(request.body, context);
    return Response200{.registered = std::move(result.registered)};
}
}  // namespace shipments::views::api::v1::shipments::route_segments::register_usage

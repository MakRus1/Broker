#include <helpers/converters/locations.hpp>
#include <platform/browser/session.hpp>
#include <views/api/geography/route_locations/view.hpp>

namespace web_bff::views::api::geography::route_locations {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transport_locations::ApiV1RouteLocationsSearch;
    const auto result = deps.transport_locations.ApiV1RouteLocationsSearch(
        api::Request{
            .body = {.kind = request.body.kind == models::shipments::TransportLocationKind::SeaPort
                    ? clients::transport_locations::models::TransportLocationKind::SeaPort
                    : clients::transport_locations::models::TransportLocationKind::Airport,
                .query = request.body.query}},
        session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back(Convert(item));
    return response;
}
}  // namespace web_bff::views::api::geography::route_locations

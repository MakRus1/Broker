#include <helpers/converters/route_locations.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/route_locations/list/view.hpp>

namespace admin_bff::views::api::admin::route_locations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiAdminRouteLocationsList;
    const auto result = deps.shipments.ApiAdminRouteLocationsList(
        api::Request{.body = {.after = request.body.after,
                         .kind = request.body.kind == models::TransportLocationKind::SeaPort
                             ? clients::shipments::models::TransportLocationKind::SeaPort
                             : clients::shipments::models::TransportLocationKind::Airport,
                         .query = request.body.query}},
        session.Access);
    Response200 response;
    response.next = result.next;
    for (const auto& item : result.items)
        response.items.push_back(Convert(item));
    return response;
}
}  // namespace admin_bff::views::api::admin::route_locations::list

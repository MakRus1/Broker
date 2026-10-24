#include <helpers/converters/route_locations.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/route_locations/save/view.hpp>

namespace admin_bff::views::api::admin::route_locations::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiAdminRouteLocationsSave;
    const auto result = deps.shipments.ApiAdminRouteLocationsSave(
        api::Request{.body = {.code = request.body.code,
                         .country_code = request.body.country_code,
                         .existing = request.body.existing,
                         .id = request.body.id,
                         .kind = request.body.kind == models::TransportLocationKind::SeaPort
                             ? clients::shipments::models::TransportLocationKind::SeaPort
                             : clients::shipments::models::TransportLocationKind::Airport,
                         .name = request.body.name}},
        session.Access);
    return Response200{.code = result.code,
        .country_code = result.country_code,
        .id = result.id,
        .kind = result.kind == clients::shipments::models::TransportLocationKind::SeaPort
            ? models::TransportLocationKind::SeaPort
            : models::TransportLocationKind::Airport,
        .name = result.name};
}
}  // namespace admin_bff::views::api::admin::route_locations::save

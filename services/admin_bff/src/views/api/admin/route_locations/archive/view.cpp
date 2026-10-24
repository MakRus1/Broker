#include <helpers/converters/route_locations.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/route_locations/archive/view.hpp>

namespace admin_bff::views::api::admin::route_locations::archive {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiAdminRouteLocationsArchive;
    const auto result = deps.shipments.ApiAdminRouteLocationsArchive(
        api::Request{.body = {.id = request.body.id}}, session.Access);
    return Response200{.id = result.id};
}
}  // namespace admin_bff::views::api::admin::route_locations::archive

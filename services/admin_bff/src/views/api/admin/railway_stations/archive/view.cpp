#include <platform/browser/session.hpp>
#include <views/api/admin/railway_stations/archive/view.hpp>

namespace admin_bff::views::api::admin::railway_stations::archive {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transport_locations::ApiAdminRailwayStationsArchive;
    const auto result = deps.transport_locations.ApiAdminRailwayStationsArchive(
        api::Request{.body = {.id = request.body.id}}, session.Access);
    return Response200{.id = result.id};
}
}  // namespace admin_bff::views::api::admin::railway_stations::archive

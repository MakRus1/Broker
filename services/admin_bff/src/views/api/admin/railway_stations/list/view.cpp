#include <helpers/registry.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/railway_stations/list/view.hpp>

namespace admin_bff::views::api::admin::railway_stations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transport_locations::ApiAdminRailwayStationsList;
    const auto result = deps.transport_locations.ApiAdminRailwayStationsList(
        api::Request{
            .body = api::RequestBody{.after = request.body.after,
                .country_code = request.body.country_code,
                .cursor =
                    ConvertRegistryCursor<clients::transport_locations::models::StationListCursor>(
                        request.body.cursor),
                .descending = request.body.descending,
                .limit = request.body.limit,
                .query = request.body.query,
                .sort = clients::transport_locations::models::ParseEnumScalar(
                    EnumValue(request.body.sort), decltype(api::RequestBody{}.sort){})}},
        session.Access);
    Response200 response;
    response.next = result.next;
    response.next_cursor = ConvertRegistryCursor<models::StationListCursor>(result.next_cursor);
    for (const auto& station : result.items)
        response.items.push_back({.country_code = station.country_code,
            .esr_code = station.esr_code,
            .name = station.name});
    return response;
}
}  // namespace admin_bff::views::api::admin::railway_stations::list

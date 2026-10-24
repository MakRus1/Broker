#include <platform/browser/session.hpp>
#include <views/api/admin/railway_stations/list/view.hpp>

namespace admin_bff::views::api::admin::railway_stations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiAdminRailwayStationsList;
    const auto result = deps.transportations.ApiAdminRailwayStationsList(
        api::Request{.body = {.query = request.body.query, .after = request.body.after}},
        session.Access);
    Response200 response;
    response.next = result.next;
    for (const auto& station : result.items)
        response.items.push_back({.country_code = station.country_code,
            .esr_code = station.esr_code,
            .name = station.name});
    return response;
}
}  // namespace admin_bff::views::api::admin::railway_stations::list

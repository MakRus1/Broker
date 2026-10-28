#include <platform/browser/session.hpp>
#include <views/api/admin/railway_stations/save/view.hpp>

namespace admin_bff::views::api::admin::railway_stations::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transport_locations::ApiAdminRailwayStationsSave;
    const auto result = deps.transport_locations.ApiAdminRailwayStationsSave(
        api::Request{.body = {.country_code = request.body.country_code,
                         .esr_code = request.body.esr_code,
                         .existing = request.body.existing,
                         .id = request.body.id,
                         .name = request.body.name,
                         .uic_code = request.body.uic_code}},
        session.Access);
    return Response200{.country_code = result.country_code,
        .esr_code = result.esr_code,
        .esr_source = result.esr_source,
        .id = result.id,
        .name = result.name,
        .uic_code = result.uic_code};
}
}  // namespace admin_bff::views::api::admin::railway_stations::save

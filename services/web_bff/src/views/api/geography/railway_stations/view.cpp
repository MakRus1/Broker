#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/geography/railway_stations/view.hpp>

namespace web_bff::views::api::geography::railway_stations {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1RailwayStationsSearch;
    const auto result = deps.transportations.ApiV1RailwayStationsSearch(
        api::Request{.body = {.query = request.body.query}}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::geography::railway_stations

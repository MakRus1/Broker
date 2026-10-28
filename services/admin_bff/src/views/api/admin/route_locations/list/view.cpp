#include <helpers/converters/route_locations.hpp>
#include <helpers/registry.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/route_locations/list/view.hpp>

namespace admin_bff::views::api::admin::route_locations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transport_locations::ApiAdminRouteLocationsList;
    const auto result = deps.transport_locations.ApiAdminRouteLocationsList(
        api::Request{
            .body = api::RequestBody{.after = request.body.after,
                .country_code = request.body.country_code,
                .cursor =
                    ConvertRegistryCursor<clients::transport_locations::models::LocationListCursor>(
                        request.body.cursor),
                .descending = request.body.descending,
                .kind = clients::transport_locations::models::ParseEnumScalar(
                    models::EnumValue(request.body.kind),
                    clients::transport_locations::models::TransportLocationKind{}),
                .limit = request.body.limit,
                .query = request.body.query,
                .sort = clients::transport_locations::models::ParseEnumScalar(
                    EnumValue(request.body.sort), decltype(api::RequestBody{}.sort){})}},
        session.Access);
    Response200 response;
    response.next = result.next;
    response.next_cursor = ConvertRegistryCursor<models::LocationListCursor>(result.next_cursor);
    for (const auto& item : result.items)
        response.items.push_back(Convert(item));
    return response;
}
}  // namespace admin_bff::views::api::admin::route_locations::list

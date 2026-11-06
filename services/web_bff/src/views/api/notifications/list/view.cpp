#include <platform/browser/session.hpp>
#include <views/api/notifications/list/view.hpp>

namespace web_bff::views::api::notifications::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::notifications::ApiV1NotificationsList;
    const auto result = deps.notifications.ApiV1NotificationsList(
        api::Request{.body = {.before = request.body.before,
                         .limit = request.body.limit,
                         .organization_id = request.body.organization_id}},
        session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back({.created_at = item.created_at,
            .id = item.id,
            .read = item.read,
            .shipment_id = item.shipment_id,
            .shipment_number = item.shipment_number,
            .shipment_revision = item.shipment_revision});
    return response;
}
}  // namespace web_bff::views::api::notifications::list

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
            .entity_id = item.entity_id,
            .entity_number = item.entity_number,
            .entity_revision = item.entity_revision,
            .event_type =
                models::ParseEnumScalar(clients::notifications::models::EnumValue(item.event_type),
                    models::Notification_event_type{}),
            .id = item.id,
            .read = item.read});
    return response;
}
}  // namespace web_bff::views::api::notifications::list

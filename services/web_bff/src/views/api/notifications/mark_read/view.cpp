#include <platform/browser/session.hpp>
#include <views/api/notifications/mark_read/view.hpp>

namespace web_bff::views::api::notifications::mark_read {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::notifications::ApiV1NotificationsMarkRead;
    const auto result = deps.notifications.ApiV1NotificationsMarkRead(
        api::Request{.body = {.dismiss = request.body.dismiss,
                         .ids = request.body.ids,
                         .organization_id = request.body.organization_id}},
        session.Access);
    return Response200{.updated = result.updated};
}
}  // namespace web_bff::views::api::notifications::mark_read

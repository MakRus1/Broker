#include <platform/browser/session.hpp>
#include <views/api/notifications/read/view.hpp>

namespace web_bff::views::api::notifications::read {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::notifications::ApiV1NotificationsRead;
    const auto result = deps.notifications.ApiV1NotificationsRead(
        api::Request{.body = {.dismiss = request.body.dismiss,
                         .ids = request.body.ids,
                         .organization_id = request.body.organization_id}},
        session.Access);
    return Response200{.updated = result.updated};
}
}  // namespace web_bff::views::api::notifications::read

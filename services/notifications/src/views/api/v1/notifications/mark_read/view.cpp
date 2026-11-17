#include <components/inbox.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/notifications/mark_read/view.hpp>

namespace notifications::views::api::v1::notifications::mark_read {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& access_token = platform::auth::GetAccessToken(context);

    return Response200{
        .updated = deps.inbox->MarkNotificationsRead(request.body, identity, access_token)};
}
}  // namespace notifications::views::api::v1::notifications::mark_read

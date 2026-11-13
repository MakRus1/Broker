#include <components/event_acceptance.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/internal/v1/events/publish/view.hpp>

namespace notifications::views::api::internal::v1::events::publish {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    deps.event_acceptance->AcceptEvent(request.body, platform::auth::GetIdentity(context));
    return Response200{.accepted = true};
}
}  // namespace notifications::views::api::internal::v1::events::publish

#include <components/inbox.hpp>
#include <views/api/v1/notifications/read/view.hpp>

namespace notifications::views::api::v1::notifications::read {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.updated = deps.inbox->Read(request.body, context)};
}
}  // namespace notifications::views::api::v1::notifications::read

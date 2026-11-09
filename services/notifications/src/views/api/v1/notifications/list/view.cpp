#include <components/inbox.hpp>
#include <views/api/v1/notifications/list/view.hpp>

namespace notifications::views::api::v1::notifications::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.items = deps.inbox->List(request.body, context)};
}
}  // namespace notifications::views::api::v1::notifications::list

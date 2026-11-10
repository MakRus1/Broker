#include <components/preferences.hpp>
#include <views/api/v1/preferences/list/view.hpp>

namespace user_preferences::views::api::v1::preferences::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.preferences = deps.preferences->List(request.body, context)};
}
}  // namespace user_preferences::views::api::v1::preferences::list

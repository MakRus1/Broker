#include <components/preferences.hpp>
#include <views/api/v1/preferences/save/view.hpp>

namespace user_preferences::views::api::v1::preferences::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.preference = deps.preferences->Save(request.body, context)};
}
}  // namespace user_preferences::views::api::v1::preferences::save

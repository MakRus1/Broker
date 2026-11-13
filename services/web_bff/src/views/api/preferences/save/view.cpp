#include <components/preferences.hpp>
#include <platform/browser/session.hpp>
#include <utility>
#include <views/api/preferences/save/view.hpp>

namespace web_bff::views::api::preferences::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    auto result = deps.preferences.SavePersonalPreference(request.body, session.Access);
    return Response200{.preference = std::move(result)};
}
}  // namespace web_bff::views::api::preferences::save

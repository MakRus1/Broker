#include <platform/browser/session.hpp>
#include <views/api/preferences/save/view.hpp>

namespace web_bff::views::api::preferences::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::user_preferences::ApiV1PreferencesSave;
    const auto result = deps.user_preferences.ApiV1PreferencesSave(
        api::Request{.body = {.expected_revision = request.body.expected_revision,
                         .key = request.body.key,
                         .organization_id = request.body.organization_id,
                         .value = request.body.value}},
        session.Access);
    return Response200{.preference = {.key = result.preference.key,
                           .revision = result.preference.revision,
                           .value = result.preference.value}};
}
}  // namespace web_bff::views::api::preferences::save

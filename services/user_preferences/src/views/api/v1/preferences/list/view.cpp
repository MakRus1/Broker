#include <components/preferences.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/preferences/list/view.hpp>

namespace user_preferences::views::api::v1::preferences::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& access_token = platform::auth::GetAccessToken(context);

    return Response200{
        .preferences = deps.preferences->ListPreferences(request.body, identity, access_token)};
}
}  // namespace user_preferences::views::api::v1::preferences::list

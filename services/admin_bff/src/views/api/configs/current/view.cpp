#include <components/browser_session.hpp>
#include <models/config_conversions.hpp>
#include <views/api/configs/current/view.hpp>

namespace admin_bff::views::api::configs::current {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsCurrent(
        {.body = {.config_name = std::move(request.body.config_name)}}, session.Access);
    return Response200{.active_draft_id = result.active_draft_id,
        .config_name = result.config_name,
        .value = result.value,
        .version = result.version};
}
}  // namespace admin_bff::views::api::configs::current

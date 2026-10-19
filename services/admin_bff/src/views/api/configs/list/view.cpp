#include <components/browser_session.hpp>
#include <models/config_conversions.hpp>
#include <views/api/configs/list/view.hpp>

namespace admin_bff::views::api::configs::list {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsList({}, session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back({.active_draft_id = item.active_draft_id,
            .config_name = item.config_name,
            .version = item.version});
    return response;
}
}  // namespace admin_bff::views::api::configs::list

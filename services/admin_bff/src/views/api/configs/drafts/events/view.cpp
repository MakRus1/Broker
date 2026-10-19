#include <components/browser_session.hpp>
#include <models/config_conversions.hpp>
#include <views/api/configs/drafts/events/view.hpp>

namespace admin_bff::views::api::configs::drafts::events {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsDraftsEvents(
        {.body = {.id = std::move(request.body.id)}}, session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back({.action = item.action,
            .actor = item.actor,
            .created_at = item.created_at,
            .snapshot = ToBrowserSnapshot(item.snapshot)});
    return response;
}
}  // namespace admin_bff::views::api::configs::drafts::events

#include <models/config_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/configs/drafts/list/view.hpp>

namespace admin_bff::views::api::configs::drafts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsDraftsList(
        {.body = {.before_id = std::move(request.body.before_id),
             .config_name = std::move(request.body.config_name)}},
        session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back(ToBrowserDraft<models::ConfigDraft>(item));
    return response;
}
}  // namespace admin_bff::views::api::configs::drafts::list

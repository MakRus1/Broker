#include <models/config_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/configs/drafts/events/view.hpp>

namespace admin_bff::views::api::configs::drafts::events {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsDraftsEvents(
        {.body = {.id = std::move(request.body.id)}}, session.Access);
    Response200 response;
    response.items = ConvertEvents<decltype(response.items)::value_type>(result.items);
    return response;
}
}  // namespace admin_bff::views::api::configs::drafts::events

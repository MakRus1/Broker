#include <models/config_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/configs/history/view.hpp>

namespace admin_bff::views::api::configs::history {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsHistory({}, session.Access);
    Response200 response;
    response.items = ConvertHistory<decltype(response.items)::value_type>(result.items);
    return response;
}
}  // namespace admin_bff::views::api::configs::history

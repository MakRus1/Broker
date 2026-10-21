#include <models/config_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/configs/list/view.hpp>

namespace admin_bff::views::api::configs::list {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsList({}, session.Access);
    Response200 response;
    response.items = ConvertCatalog<decltype(response.items)::value_type>(result.items);
    return response;
}
}  // namespace admin_bff::views::api::configs::list

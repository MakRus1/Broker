#include <models/config_conversions.hpp>
#include <platform/browser/session.hpp>
#include <views/api/configs/drafts/approve/view.hpp>

namespace admin_bff::views::api::configs::drafts::approve {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsDraftsApprove(
        {.body = {.expected_version = std::move(request.body.expected_version),
             .id = std::move(request.body.id)}},
        session.Access);
    return ToBrowserDraft<Response200>(result);
}
}  // namespace admin_bff::views::api::configs::drafts::approve

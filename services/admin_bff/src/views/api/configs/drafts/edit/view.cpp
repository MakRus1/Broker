#include <components/browser_session.hpp>
#include <models/config_conversions.hpp>
#include <views/api/configs/drafts/edit/view.hpp>

namespace admin_bff::views::api::configs::drafts::edit {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsDraftsEdit(
        {.body = {.description = std::move(request.body.description),
             .expected_version = std::move(request.body.expected_version),
             .id = std::move(request.body.id),
             .value = std::move(request.body.value)}},
        session.Access);
    return ToBrowserDraft<Response200>(result);
}
}  // namespace admin_bff::views::api::configs::drafts::edit

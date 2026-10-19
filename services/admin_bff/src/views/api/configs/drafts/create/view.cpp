#include <components/browser_session.hpp>
#include <models/config_conversions.hpp>
#include <views/api/configs/drafts/create/view.hpp>

namespace admin_bff::views::api::configs::drafts::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = GetBrowserSession(context);
    const auto result = deps.config_admin.AdminConfigsDraftsCreate(
        {.body = {.config_name = std::move(request.body.config_name),
             .description = std::move(request.body.description),
             .expected_config_version = std::move(request.body.expected_config_version),
             .source_revision = std::move(request.body.source_revision),
             .value = std::move(request.body.value)}},
        session.Access);
    return ToBrowserDraft<Response201>(result);
}
}  // namespace admin_bff::views::api::configs::drafts::create

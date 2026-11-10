#include <converters/localization.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/localization/get/view.hpp>

namespace admin_bff::views::api::admin::localization::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::localization::ApiAdminLocalizationGet;
    const auto& session = platform::browser::GetBrowserSession(context);
    const api::RequestBody body{.organization_id = request.body.organization_id};
    const auto result =
        deps.localization.ApiAdminLocalizationGet(api::Request{.body = body}, session.Access);
    return converters::Convert<Response200>(result);
}
}  // namespace admin_bff::views::api::admin::localization::get

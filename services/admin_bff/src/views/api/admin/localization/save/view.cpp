#include <converters/localization.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/localization/save/view.hpp>

namespace admin_bff::views::api::admin::localization::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::localization::ApiAdminLocalizationSave;
    const auto& session = platform::browser::GetBrowserSession(context);
    const api::RequestBody body{.catalog = converters::Convert(request.body.catalog),
        .organization_id = request.body.organization_id,
        .version = request.body.version};
    const auto result =
        deps.localization.ApiAdminLocalizationSave(api::Request{.body = body}, session.Access);
    return converters::Convert<Response200>(result);
}
}  // namespace admin_bff::views::api::admin::localization::save

#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/sales/settings/get/view.hpp>

namespace admin_bff::views::api::admin::sales::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiAdminSalesSettingsGet(
        {.body = {.organization_id = request.body.organization_id}}, session.Access);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = ToAdminSalesStages(result.stages)};
}
}  // namespace admin_bff::views::api::admin::sales::settings::get

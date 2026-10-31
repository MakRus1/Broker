#include <platform/browser/session.hpp>
#include <views/api/admin/finance/settings/get/view.hpp>

namespace admin_bff::views::api::admin::finance::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result = deps.finance.ApiAdminFinanceSettingsGet(
        {.body = {.organization_id = body.organization_id}}, session.Access);
    return Response200{.incoming_number_template = result.incoming_number_template,
        .outgoing_number_template = result.outgoing_number_template,
        .reporting_currency = result.reporting_currency,
        .reset_numbers_yearly = result.reset_numbers_yearly,
        .revision = result.revision};
}
}  // namespace admin_bff::views::api::admin::finance::settings::get

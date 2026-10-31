#include <platform/browser/session.hpp>
#include <views/api/admin/finance/settings/update/view.hpp>

namespace admin_bff::views::api::admin::finance::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result = deps.finance.ApiAdminFinanceSettingsUpdate(
        {.body = {.expected_revision = body.expected_revision,
             .incoming_number_template = body.incoming_number_template,
             .organization_id = body.organization_id,
             .outgoing_number_template = body.outgoing_number_template,
             .reporting_currency = body.reporting_currency,
             .reset_numbers_yearly = body.reset_numbers_yearly}},
        session.Access);
    return Response200{.incoming_number_template = result.incoming_number_template,
        .outgoing_number_template = result.outgoing_number_template,
        .reporting_currency = result.reporting_currency,
        .reset_numbers_yearly = result.reset_numbers_yearly,
        .revision = result.revision};
}
}  // namespace admin_bff::views::api::admin::finance::settings::update

#include <platform/browser/session.hpp>
#include <views/api/v1/finance/settings/get/view.hpp>

namespace web_bff::views::api::v1::finance::settings::get {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.finance.ApiV1FinanceSettingsGet({}, session.Access);
    return Response200{.incoming_number_template = result.incoming_number_template,
        .outgoing_number_template = result.outgoing_number_template,
        .reporting_currency = result.reporting_currency,
        .reset_numbers_yearly = result.reset_numbers_yearly,
        .revision = result.revision};
}
}  // namespace web_bff::views::api::v1::finance::settings::get

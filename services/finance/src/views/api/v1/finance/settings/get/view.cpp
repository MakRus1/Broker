#include <helpers/finance_settings.hpp>
#include <views/api/v1/finance/settings/get/view.hpp>

namespace finance::views::api::v1::finance::settings::get {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto result = helpers::GetFinanceSettings(deps, organization);
    return Response200{.incoming_number_template = result.incoming_number_template,
        .outgoing_number_template = result.outgoing_number_template,
        .reporting_currency = result.reporting_currency,
        .reset_numbers_yearly = result.reset_numbers_yearly,
        .revision = result.revision};
}
}  // namespace finance::views::api::v1::finance::settings::get

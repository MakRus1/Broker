#include <helpers/finance_settings.hpp>
#include <views/api/v1/finance/settings/get/view.hpp>

namespace finance::views::api::v1::finance::settings::get {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto result = helpers::GetFinanceSettings(deps, organization);
    return Response200{
        .reporting_currency = result.reporting_currency, .revision = result.revision};
}
}  // namespace finance::views::api::v1::finance::settings::get

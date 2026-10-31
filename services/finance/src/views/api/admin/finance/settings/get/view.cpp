#include <helpers/finance_settings.hpp>
#include <views/api/admin/finance/settings/get/view.hpp>

namespace finance::views::api::admin::finance::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    helpers::RequireFinanceAdministrator(deps, context, request.body.organization_id);
    const auto result = helpers::GetFinanceSettings(deps, request.body.organization_id);
    return Response200{.incoming_number_template = result.incoming_number_template,
        .outgoing_number_template = result.outgoing_number_template,
        .reporting_currency = result.reporting_currency,
        .reset_numbers_yearly = result.reset_numbers_yearly,
        .revision = result.revision};
}
}  // namespace finance::views::api::admin::finance::settings::get

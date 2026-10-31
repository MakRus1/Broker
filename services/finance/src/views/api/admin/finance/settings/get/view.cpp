#include <helpers/finance_settings.hpp>
#include <views/api/admin/finance/settings/get/view.hpp>

namespace finance::views::api::admin::finance::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    helpers::RequireFinanceAdministrator(deps, context, request.body.organization_id);
    const auto result = helpers::GetFinanceSettings(deps, request.body.organization_id);
    return Response200{
        .reporting_currency = result.reporting_currency, .revision = result.revision};
}
}  // namespace finance::views::api::admin::finance::settings::get

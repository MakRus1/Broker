#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/sales/settings/update/view.hpp>

namespace admin_bff::views::api::admin::sales::settings::update {
using converters::ToAdminSalesStages;
using converters::ToServiceSalesStages;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiAdminSalesSettingsUpdate(
        {.body = {.expected_revision = request.body.expected_revision,
             .inquiry_number_template = request.body.inquiry_number_template,
             .organization_id = request.body.organization_id,
             .reset_number_yearly = request.body.reset_number_yearly,
             .stages = ToServiceSalesStages(request.body.stages)}},
        session.Access);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = ToAdminSalesStages(result.stages)};
}
}  // namespace admin_bff::views::api::admin::sales::settings::update

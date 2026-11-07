#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/admin/sales/settings/update/view.hpp>

namespace admin_bff::views::api::admin::sales::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiAdminSalesSettingsUpdate;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiAdminSalesSettingsUpdate(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = ConvertSales<std::vector<models::sales::SalesStage>>(result.stages)};
}
}  // namespace admin_bff::views::api::admin::sales::settings::update

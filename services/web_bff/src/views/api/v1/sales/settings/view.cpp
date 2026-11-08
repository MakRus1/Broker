#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/settings/view.hpp>

namespace web_bff::views::api::v1::sales::settings {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesSettings;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesSettings(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = ConvertSales<std::vector<models::sales::SalesStage>>(result.stages)};
}
}  // namespace web_bff::views::api::v1::sales::settings

#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/settings/view.hpp>

namespace web_bff::views::api::v1::sales::settings {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesSettings;
    namespace api = clients::sales::ApiV1SalesSettings;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesSettings(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = Convert(result.stages)};
}
}  // namespace web_bff::views::api::v1::sales::settings

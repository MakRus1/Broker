#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/financial_labels/get/view.hpp>

namespace web_bff::views::api::v1::financial_labels::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::finance::ApiV1FinancialLabelsGet;
    const auto result = deps.finance.ApiV1FinancialLabelsGet(
        api::Request{.body = {.id = request.body.id}}, session.Access);
    return Response200{.label = Convert(result.label)};
}
}  // namespace web_bff::views::api::v1::financial_labels::get

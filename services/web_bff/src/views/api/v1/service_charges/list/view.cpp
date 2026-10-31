#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/service_charges/list/view.hpp>

namespace web_bff::views::api::v1::service_charges::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::finance::ApiV1ServiceChargesList;
    const auto result = deps.finance.ApiV1ServiceChargesList(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.invoices = Convert(result.invoices),
        .items = Convert(result.items),
        .next_after = result.next_after,
        .reporting = Convert(result.reporting),
        .summary = Convert(result.summary)};
}
}  // namespace web_bff::views::api::v1::service_charges::list

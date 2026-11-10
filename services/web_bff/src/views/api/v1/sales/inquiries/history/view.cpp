#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/history/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::history {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesHistory;
    namespace api = clients::sales::ApiV1SalesInquiriesHistory;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesHistory(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::history

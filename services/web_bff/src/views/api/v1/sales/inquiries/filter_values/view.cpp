#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/filter_values/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::filter_values {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesFilterValues;
    namespace api = clients::sales::ApiV1SalesInquiriesFilterValues;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesFilterValues(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.fields = Convert(result.fields),
        .next_after = result.next_after,
        .values = Convert(result.values)};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::filter_values

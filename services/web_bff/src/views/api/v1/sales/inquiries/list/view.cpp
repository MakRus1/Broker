#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/list/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::list {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesList;
    namespace api = clients::sales::ApiV1SalesInquiriesList;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesList(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.items = Convert(result.items),
        .next_cursor = result.next_cursor.has_value()
            ? std::optional{Convert(result.next_cursor.value())}
            : std::nullopt};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::list

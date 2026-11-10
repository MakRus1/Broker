#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/contacts/list/view.hpp>

namespace web_bff::views::api::v1::sales::contacts::list {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesContactsList;
    namespace api = clients::sales::ApiV1SalesContactsList;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesContactsList(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.items = Convert(result.items), .next_before = result.next_before};
}
}  // namespace web_bff::views::api::v1::sales::contacts::list

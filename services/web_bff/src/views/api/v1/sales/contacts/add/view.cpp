#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/contacts/add/view.hpp>

namespace web_bff::views::api::v1::sales::contacts::add {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesContactsAdd;
    namespace api = clients::sales::ApiV1SalesContactsAdd;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesContactsAdd(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.card = Convert(result.card), .item = Convert(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::contacts::add

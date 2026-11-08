#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/contacts/list/view.hpp>

namespace web_bff::views::api::v1::sales::contacts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesContactsList;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesContactsList(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{
        .items = ConvertSales<std::vector<models::sales::SalesContact>>(result.items),
        .next_before = result.next_before};
}
}  // namespace web_bff::views::api::v1::sales::contacts::list

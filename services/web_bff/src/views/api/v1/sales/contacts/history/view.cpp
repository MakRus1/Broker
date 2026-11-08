#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/contacts/history/view.hpp>

namespace web_bff::views::api::v1::sales::contacts::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesContactsHistory;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesContactsHistory(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{
        .items = ConvertSales<std::vector<models::sales::SalesContactHistoryEntry>>(result.items)};
}
}  // namespace web_bff::views::api::v1::sales::contacts::history

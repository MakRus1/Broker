#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/rfqs/history/view.hpp>

namespace web_bff::views::api::v1::sales::rfqs::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesRfqsHistory;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesRfqsHistory(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{
        .items = ConvertSales<std::vector<models::sales::RfqHistoryEntry>>(result.items)};
}
}  // namespace web_bff::views::api::v1::sales::rfqs::history

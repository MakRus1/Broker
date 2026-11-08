#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/history/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesHistory;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesHistory(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{
        .items = ConvertSales<std::vector<models::sales::InquiryHistoryEntry>>(result.items)};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::history

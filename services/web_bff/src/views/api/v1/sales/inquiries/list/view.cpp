#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/list/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesList;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesList(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.items = ConvertSales<std::vector<models::sales::Inquiry>>(result.items),
        .next_cursor = result.next_cursor.has_value()
            ? std::optional{ConvertSales<models::sales::SalesRegistryCursor>(
                  result.next_cursor.value())}
            : std::nullopt};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::list

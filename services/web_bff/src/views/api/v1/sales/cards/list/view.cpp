#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/cards/list/view.hpp>

namespace web_bff::views::api::v1::sales::cards::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesCardsList;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesCardsList(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.items = ConvertSales<std::vector<models::sales::SalesCard>>(result.items),
        .next_cursor = result.next_cursor.has_value()
            ? std::optional{ConvertSales<models::sales::SalesRegistryCursor>(
                  result.next_cursor.value())}
            : std::nullopt};
}
}  // namespace web_bff::views::api::v1::sales::cards::list

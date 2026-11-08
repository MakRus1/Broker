#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/cards/get/view.hpp>

namespace web_bff::views::api::v1::sales::cards::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesCardsGet;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesCardsGet(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.item = ConvertSales<models::sales::SalesCard>(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::cards::get

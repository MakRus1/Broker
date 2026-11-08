#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/contacts/add/view.hpp>

namespace web_bff::views::api::v1::sales::contacts::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesContactsAdd;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesContactsAdd(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.card = ConvertSales<models::sales::SalesCard>(result.card),
        .item = ConvertSales<models::sales::SalesContact>(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::contacts::add

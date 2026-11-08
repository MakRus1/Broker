#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/contacts/update/view.hpp>

namespace web_bff::views::api::v1::sales::contacts::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesContactsUpdate;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesContactsUpdate(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.item = ConvertSales<models::sales::SalesContact>(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::contacts::update

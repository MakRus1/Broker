#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/filter_values/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesFilterValues;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesFilterValues(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{
        .fields = ConvertSales<std::vector<models::sales::RegistryField>>(result.fields),
        .next_after = result.next_after,
        .values = ConvertSales<std::vector<models::sales::RegistryValue>>(result.values)};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::filter_values

#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/inquiries/save/view.hpp>

namespace web_bff::views::api::v1::sales::inquiries::save {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesInquiriesSave;
    namespace api = clients::sales::ApiV1SalesInquiriesSave;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesInquiriesSave(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.item = Convert(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::inquiries::save

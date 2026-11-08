#include <helpers/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/proposals/save/view.hpp>

namespace web_bff::views::api::v1::sales::proposals::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesProposalsSave;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesProposalsSave(
        {.body = api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue())},
        session.Access);
    return Response200{.item = ConvertSales<models::sales::Proposal>(result.item)};
}
}  // namespace web_bff::views::api::v1::sales::proposals::save

#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/financial_entries/list/view.hpp>

namespace web_bff::views::api::v1::financial_entries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::finance::ApiV1FinancialEntriesList;
    const auto result = deps.finance.ApiV1FinancialEntriesList(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.items = Convert(result.items),
        .next_after = result.next_after,
        .summary = Convert(result.summary)};
}
}  // namespace web_bff::views::api::v1::financial_entries::list

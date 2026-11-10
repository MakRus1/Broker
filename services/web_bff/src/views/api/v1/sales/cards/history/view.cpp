#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/cards/history/view.hpp>

namespace web_bff::views::api::v1::sales::cards::history {
using namespace ::web_bff::converters::sales;

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::sales::ApiV1SalesCardsHistory;
    namespace api = clients::sales::ApiV1SalesCardsHistory;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesCardsHistory(
        {.body = Convert<api::RequestBody>(request.body)}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::v1::sales::cards::history

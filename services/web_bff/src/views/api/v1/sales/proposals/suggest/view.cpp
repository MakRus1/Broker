#include <converters/sales.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/sales/proposals/suggest/view.hpp>

namespace web_bff::views::api::v1::sales::proposals::suggest {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.sales.ApiV1SalesProposalsSuggest(
        {.body = {.calculation_date = request.body.calculation_date,
             .inquiry_id = request.body.inquiry_id,
             .strategy = converters::sales::Convert(request.body.strategy)}},
        session.Access);
    return Response200{.gaps = converters::sales::Convert(result.gaps),
        .items = converters::sales::Convert(result.items),
        .selected = converters::sales::Convert(result.selected)};
}
}  // namespace web_bff::views::api::v1::sales::proposals::suggest

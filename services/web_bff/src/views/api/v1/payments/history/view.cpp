#include <helpers/converters.hpp>
#include <helpers/converters/financial_history.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/payments/history/view.hpp>

namespace web_bff::views::api::v1::payments::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    clients::finance::ApiV1PaymentsHistory::RequestBody body{
        .id = request.body.id, .limit = request.body.limit};
    if (request.body.before.has_value())
        body.before = Convert(request.body.before.value());
    const auto value = deps.finance.ApiV1PaymentsHistory({.body = body}, session.Access);
    Response200 result{.items = Convert(value.items)};
    if (value.next_before.has_value())
        result.next_before = Convert(value.next_before.value());
    return result;
}
}  // namespace web_bff::views::api::v1::payments::history

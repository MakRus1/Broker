#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/payments/payment_candidates/view.hpp>

namespace web_bff::views::api::v1::payments::payment_candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1PaymentsPaymentCandidates({.body = {.after = request.body.after,
                                                         .invoice_id = request.body.invoice_id,
                                                         .limit = request.body.limit,
                                                         .query = request.body.query}},
            session.Access);
    return Response200{.items = Convert(result.items), .next_after = result.next_after};
}
}  // namespace web_bff::views::api::v1::payments::payment_candidates

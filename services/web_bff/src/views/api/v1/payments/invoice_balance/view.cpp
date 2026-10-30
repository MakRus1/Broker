#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/payments/invoice_balance/view.hpp>

namespace web_bff::views::api::v1::payments::invoice_balance {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1PaymentsInvoiceBalance({.body = {.id = request.body.id}}, session.Access);
    return Response200{.invoice = Convert(result.invoice),
        .outstanding_minor = result.outstanding_minor,
        .paid_minor = result.paid_minor,
        .payments = Convert(result.payments)};
}
}  // namespace web_bff::views::api::v1::payments::invoice_balance

#include <components/payments.hpp>
#include <utility>
#include <views/api/v1/payments/invoice_balance/view.hpp>

namespace finance::views::api::v1::payments::invoice_balance {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.payments->InvoiceBalance(request.body, context);
    return Response200{.invoice = std::move(result.invoice),
        .outstanding_minor = std::move(result.outstanding_minor),
        .paid_minor = std::move(result.paid_minor),
        .payments = std::move(result.payments)};
}
}  // namespace finance::views::api::v1::payments::invoice_balance

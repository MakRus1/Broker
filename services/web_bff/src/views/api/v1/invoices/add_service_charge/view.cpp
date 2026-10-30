#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/invoices/add_service_charge/view.hpp>

namespace web_bff::views::api::v1::invoices::add_service_charge {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.finance.ApiV1InvoicesAddServiceCharge(
        {.body = {.service_charge_id = request.body.service_charge_id,
             .expected_revision = request.body.expected_revision,
             .invoice_id = request.body.invoice_id,
             .previous_invoice_id = request.body.previous_invoice_id}},
        session.Access);
    return Response200{.charges = Convert(result.charges), .invoice = Convert(result.invoice)};
}
}  // namespace web_bff::views::api::v1::invoices::add_service_charge

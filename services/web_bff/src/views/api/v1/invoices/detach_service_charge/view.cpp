#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/invoices/detach_service_charge/view.hpp>

namespace web_bff::views::api::v1::invoices::detach_service_charge {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result = deps.finance.ApiV1InvoicesDetachServiceCharge(
        {.body = {.service_charge_id = request.body.service_charge_id,
             .invoice_id = request.body.invoice_id}},
        session.Access);
    return Response200{.charges = Convert(result.charges), .invoice = Convert(result.invoice)};
}
}  // namespace web_bff::views::api::v1::invoices::detach_service_charge

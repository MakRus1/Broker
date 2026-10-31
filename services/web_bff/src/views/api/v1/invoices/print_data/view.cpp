#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/invoices/print_data/view.hpp>

namespace web_bff::views::api::v1::invoices::print_data {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto value = deps.finance.ApiV1InvoicesPrintData(
        {.body = {.id = request.body.id, .revision = request.body.revision}}, session.Access);
    Response200 result{.charges = Convert(value.charges),
        .customer = Convert(value.customer),
        .invoice = Convert(value.invoice),
        .supplier = Convert(value.supplier),
        .supplier_bank = std::nullopt};
    if (value.supplier_bank.has_value())
        result.supplier_bank = Convert(value.supplier_bank.value());
    return result;
}
}  // namespace web_bff::views::api::v1::invoices::print_data

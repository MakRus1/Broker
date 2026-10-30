#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/invoices/get/view.hpp>

namespace web_bff::views::api::v1::invoices::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1InvoicesGet({.body = Convert(request.body)}, session.Access);
    return Response200{.charges = Convert(result.charges), .invoice = Convert(result.invoice)};
}
}  // namespace web_bff::views::api::v1::invoices::get

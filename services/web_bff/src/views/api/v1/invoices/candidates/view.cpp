#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/invoices/candidates/view.hpp>

namespace web_bff::views::api::v1::invoices::candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1InvoicesCandidates({.body = Convert(request.body)}, session.Access);
    return Response200{.items = Convert(result.items), .next_after = result.next_after};
}
}  // namespace web_bff::views::api::v1::invoices::candidates

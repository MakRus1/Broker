#include <components/payments.hpp>
#include <utility>
#include <views/api/v1/payments/invoice_candidates/view.hpp>

namespace finance::views::api::v1::payments::invoice_candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.payments->InvoiceCandidates(request.body, context);
    return Response200{
        .items = std::move(result.items), .next_after = std::move(result.next_after)};
}
}  // namespace finance::views::api::v1::payments::invoice_candidates

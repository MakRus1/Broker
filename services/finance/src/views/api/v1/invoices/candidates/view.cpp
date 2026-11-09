#include <components/invoices.hpp>
#include <utility>
#include <views/api/v1/invoices/candidates/view.hpp>

namespace finance::views::api::v1::invoices::candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.invoices->Candidates(request.body, context);
    return Response200{
        .items = std::move(result.items), .next_after = std::move(result.next_after)};
}
}  // namespace finance::views::api::v1::invoices::candidates

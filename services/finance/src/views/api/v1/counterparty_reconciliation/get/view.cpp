#include <components/settlements.hpp>
#include <utility>
#include <views/api/v1/counterparty_reconciliation/get/view.hpp>

namespace finance::views::api::v1::counterparty_reconciliation::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.settlements->Reconciliation(request.body, context);
    return Response200{
        .operations = std::move(result.operations), .totals = std::move(result.totals)};
}
}  // namespace finance::views::api::v1::counterparty_reconciliation::get

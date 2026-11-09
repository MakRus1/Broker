#include <components/financial_history.hpp>
#include <views/api/v1/invoices/history/view.hpp>

namespace finance::views::api::v1::invoices::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto value =
        deps.history->Load(context, request.body, models::FinancialHistorySource::Invoice);
    return Response200{.items = value.items, .next_before = value.next_before};
}
}  // namespace finance::views::api::v1::invoices::history

#include <helpers/history/load.hpp>
#include <views/api/v1/payments/history/view.hpp>

namespace finance::views::api::v1::payments::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto value = ::finance::helpers::history::Load(
        deps, context, request.body, models::FinancialHistorySource::Payment);
    return Response200{.items = value.items, .next_before = value.next_before};
}
}  // namespace finance::views::api::v1::payments::history

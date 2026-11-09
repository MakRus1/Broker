#include <components/settlements.hpp>
#include <utility>
#include <views/api/v1/counterparty_settlements/get/view.hpp>

namespace finance::views::api::v1::counterparty_settlements::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.settlements->Balances(request.body, context);
    return Response200{.totals = std::move(result.totals)};
}
}  // namespace finance::views::api::v1::counterparty_settlements::get

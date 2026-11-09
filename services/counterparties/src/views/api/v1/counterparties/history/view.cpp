#include <components/counterparties.hpp>
#include <utility>
#include <views/api/v1/counterparties/history/view.hpp>

namespace counterparties::views::api::v1::counterparties::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cards->History(request.body, context);
    return Response200{.items = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::history

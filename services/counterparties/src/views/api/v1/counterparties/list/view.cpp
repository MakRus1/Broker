#include <components/counterparties.hpp>
#include <utility>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cards->List(request.body, context);
    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace counterparties::views::api::v1::counterparties::list

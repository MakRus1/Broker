#include <components/contracts.hpp>
#include <utility>
#include <views/api/v1/contracts/list/view.hpp>

namespace contracts::views::api::v1::contracts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.documents->List(request.body, context);
    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace contracts::views::api::v1::contracts::list

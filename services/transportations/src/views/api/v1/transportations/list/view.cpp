#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->List(std::move(request.body), context);
    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace transportations::views::api::v1::transportations::list

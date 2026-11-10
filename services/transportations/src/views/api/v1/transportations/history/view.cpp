#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/history/view.hpp>

namespace transportations::views::api::v1::transportations::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->History(std::move(request.body), context);
    return Response200{.items = std::move(result.items)};
}
}  // namespace transportations::views::api::v1::transportations::history

#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/create/view.hpp>

namespace transportations::views::api::v1::transportations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->Create(std::move(request.body), context);
    return Response200{.transportation = std::move(result.transportation)};
}
}  // namespace transportations::views::api::v1::transportations::create

#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/update/view.hpp>

namespace transportations::views::api::v1::transportations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->Update(std::move(request.body), context);
    return Response200{.transportation = std::move(result.transportation)};
}
}  // namespace transportations::views::api::v1::transportations::update

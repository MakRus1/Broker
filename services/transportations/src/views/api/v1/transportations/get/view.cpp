#include <components/transportations.hpp>
#include <utility>
#include <views/api/v1/transportations/get/view.hpp>

namespace transportations::views::api::v1::transportations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportations->Get(std::move(request.body), context);
    return Response200{.transportation = std::move(result.transportation)};
}
}  // namespace transportations::views::api::v1::transportations::get

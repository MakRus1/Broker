#include <components/actuals.hpp>
#include <utility>
#include <views/api/v1/transportation_actuals/correct/view.hpp>

namespace transportations::views::api::v1::transportation_actuals::correct {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.actuals->Correct(std::move(request.body), context);
    return Response200{.transportation = std::move(result.transportation)};
}
}  // namespace transportations::views::api::v1::transportation_actuals::correct

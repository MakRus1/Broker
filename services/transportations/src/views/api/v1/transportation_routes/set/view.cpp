#include <components/routes.hpp>
#include <utility>
#include <views/api/v1/transportation_routes/set/view.hpp>

namespace transportations::views::api::v1::transportation_routes::set {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.routes->Set(std::move(request.body), context);
    return Response200{.transportation = std::move(result.transportation)};
}
}  // namespace transportations::views::api::v1::transportation_routes::set

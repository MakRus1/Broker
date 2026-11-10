#include <components/routes.hpp>
#include <utility>
#include <views/api/internal/v1/shipments/route_projection/view.hpp>

namespace transportations::views::api::internal::v1::shipments::route_projection {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.routes->Project(std::move(request.body), context);
    return Response200{.applied = std::move(result.applied)};
}
}  // namespace transportations::views::api::internal::v1::shipments::route_projection

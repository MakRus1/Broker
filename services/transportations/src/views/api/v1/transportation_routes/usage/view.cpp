#include <components/routes.hpp>
#include <utility>
#include <views/api/v1/transportation_routes/usage/view.hpp>

namespace transportations::views::api::v1::transportation_routes::usage {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.routes->Usage(std::move(request.body), context);
    return Response200{.segment_ids = std::move(result.segment_ids)};
}
}  // namespace transportations::views::api::v1::transportation_routes::usage

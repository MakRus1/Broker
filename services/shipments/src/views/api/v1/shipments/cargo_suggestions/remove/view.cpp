#include <components/cargo_suggestions.hpp>
#include <utility>
#include <views/api/v1/shipments/cargo_suggestions/remove/view.hpp>

namespace shipments::views::api::v1::shipments::cargo_suggestions::remove {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cargo_suggestions->Remove(request.body, context);
    return Response200{.deleted = std::move(result.deleted)};
}
}  // namespace shipments::views::api::v1::shipments::cargo_suggestions::remove

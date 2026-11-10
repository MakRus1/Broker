#include <components/cargo_suggestions.hpp>
#include <utility>
#include <views/api/v1/shipments/cargo_suggestions/view.hpp>

namespace shipments::views::api::v1::shipments::cargo_suggestions {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.cargo_suggestions->List(request.body, context);
    return Response200{.items = std::move(result.items)};
}
}  // namespace shipments::views::api::v1::shipments::cargo_suggestions

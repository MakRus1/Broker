#include <components/shipments.hpp>
#include <utility>
#include <views/api/v1/shipments/filter_values/view.hpp>

namespace shipments::views::api::v1::shipments::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->FilterValues(request.body, context);
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace shipments::views::api::v1::shipments::filter_values
